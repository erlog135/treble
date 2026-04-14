package net.loganhead.treble.listener

import android.Manifest
import android.annotation.SuppressLint
import android.content.Context
import android.content.pm.PackageManager
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.os.Process
import androidx.core.app.ActivityCompat
import com.shazam.shazamkit.AudioSampleRateInHz
import com.shazam.shazamkit.DeveloperToken
import com.shazam.shazamkit.DeveloperTokenProvider
import com.shazam.shazamkit.MatchResult
import com.shazam.shazamkit.ShazamKit
import com.shazam.shazamkit.ShazamKitResult
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeoutOrNull
import kotlinx.coroutines.flow.firstOrNull

data class SongResult(
    val isSuccess: Boolean,
    val title: String = "",
    val artist: String = "",
    val error: String? = null
)

class ShazamManager(private val context: Context) {

    fun hasMicrophonePermission(): Boolean {
        return ActivityCompat.checkSelfPermission(
            context,
            Manifest.permission.RECORD_AUDIO
        ) == PackageManager.PERMISSION_GRANTED
    }

    @SuppressLint("MissingPermission")
    suspend fun recognizeMusic(): SongResult = withContext(Dispatchers.IO) {
        if (!hasMicrophonePermission()) {
            return@withContext SongResult(false, error = "Microphone permission denied.")
        }

        val tokenProvider = DeveloperTokenProvider {
            DeveloperToken(BuildConfig.SHAZAM_JWT)
        }

        val catalog = ShazamKit.createShazamCatalog(tokenProvider)

        val sampleRate = 48000
        val bufferSize = AudioRecord.getMinBufferSize(
            sampleRate,
            AudioFormat.CHANNEL_IN_MONO,
            AudioFormat.ENCODING_PCM_16BIT
        )

        // 1. Initialize Streaming Session instead of Signature Generator
        val streamingSessionResult = ShazamKit.createStreamingSession(
            catalog,
            AudioSampleRateInHz.SAMPLE_RATE_48000,
            bufferSize
        )

        if (streamingSessionResult is ShazamKitResult.Failure<*>) {
            return@withContext SongResult(false, error = "Failed to create Streaming Session.")
        }
        val streamingSession = (streamingSessionResult as ShazamKitResult.Success<*>).data as com.shazam.shazamkit.StreamingSession

        // 2. Use MIC to take advantage of Android's internal noise reduction and AGC
        val audioFormat = AudioFormat.Builder()
            .setChannelMask(AudioFormat.CHANNEL_IN_MONO)
            .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
            .setSampleRate(sampleRate)
            .build()

        val audioRecord = try {
            AudioRecord.Builder()
                .setAudioSource(MediaRecorder.AudioSource.MIC)
                .setAudioFormat(audioFormat)
                .build()
        } catch (e: SecurityException) {
            return@withContext SongResult(false, error = "SecurityException: ${e.message}")
        }

        if (audioRecord.state != AudioRecord.STATE_INITIALIZED) {
            return@withContext SongResult(false, error = "AudioRecord failed to initialize.")
        }

        Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_AUDIO)

        try {
            audioRecord.startRecording()

            // 3. Wrap everything in a Timeout so it doesn't listen forever if there's no music
            val finalResult = withTimeoutOrNull(15_000L) { // Listen for a max of 15 seconds

                // Launch a parallel job to constantly feed the microphone into ShazamKit
                val readerJob = launch(Dispatchers.IO) {
                    val readBuffer = ByteArray(bufferSize)
                    while (isActive) {
                        val actualRead = audioRecord.read(readBuffer, 0, bufferSize)
                        if (actualRead > 0) {
                            // Push incremental samples directly to the API
                            streamingSession.matchStream(readBuffer, actualRead, System.currentTimeMillis())
                        }
                    }
                }

                // 4. Collect the results stream.
                // firstOrNull will suspend until the condition is met.
                // We ignore NoMatch (keep listening) and stop on Match or Error.
                val matchResult = streamingSession.recognitionResults().firstOrNull {
                    it is MatchResult.Match || it is MatchResult.Error
                }

                // Stop the microphone reader now that we have a definitive result
                readerJob.cancel()

                // 5. Evaluate the result
                when (matchResult) {
                    is MatchResult.Match -> {
                        val topMatch = matchResult.matchedMediaItems.firstOrNull()
                        if (topMatch != null) {
                            SongResult(true, topMatch.title ?: "Unknown", topMatch.artist ?: "Unknown")
                        } else {
                            SongResult(false, error = "Match found, but media data was empty.")
                        }
                    }
                    is MatchResult.Error -> {
                        SongResult(false, error = "Shazam API Error: ${matchResult.exception.message}")
                    }
                    else -> {
                        SongResult(false, error = "Unknown flow state.")
                    }
                }
            }

            // Return the result, or handle the timeout if withTimeoutOrNull returns null
            return@withContext finalResult ?: SongResult(false, error = "Listening timed out. No music found.")

        } catch (e: Exception) {
            return@withContext SongResult(false, error = "Exception: ${e.localizedMessage}")
        } finally {
            try {
                if (audioRecord.recordingState == AudioRecord.RECORDSTATE_RECORDING) {
                    audioRecord.stop()
                }
            } catch (_: Exception) {}
            audioRecord.release()
            Process.setThreadPriority(Process.THREAD_PRIORITY_DEFAULT)
        }
    }
}