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
import kotlinx.coroutines.withContext
import java.nio.ByteBuffer

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

        // 1. Initialize the Developer Token Provider
        val tokenProvider = DeveloperTokenProvider {
            DeveloperToken(BuildConfig.SHAZAM_JWT)
        }

        // 2. Setup the Catalog and Signature Generator
        val catalog = ShazamKit.createShazamCatalog(tokenProvider)

        val signatureGeneratorResult = ShazamKit.createSignatureGenerator(AudioSampleRateInHz.SAMPLE_RATE_48000)
        if (signatureGeneratorResult is ShazamKitResult.Failure<*>) {
            return@withContext SongResult(false, error = "Failed to create Signature Generator.")
        }
        val signatureGenerator = (signatureGeneratorResult as ShazamKitResult.Success<*>).data as com.shazam.shazamkit.SignatureGenerator

        // 3. Setup AudioRecord properties
        val sampleRate = 48000
        val audioFormat = AudioFormat.Builder()
            .setChannelMask(AudioFormat.CHANNEL_IN_MONO)
            .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
            .setSampleRate(sampleRate)
            .build()

        val bufferSize = AudioRecord.getMinBufferSize(
            sampleRate,
            AudioFormat.CHANNEL_IN_MONO,
            AudioFormat.ENCODING_PCM_16BIT
        )

        val audioRecord = try {
            AudioRecord.Builder()
                .setAudioSource(MediaRecorder.AudioSource.UNPROCESSED)
                .setAudioFormat(audioFormat)
                .build()
        } catch (e: SecurityException) {
            return@withContext SongResult(false, error = "SecurityException: ${e.message}")
        }

        if (audioRecord.state != AudioRecord.STATE_INITIALIZED) {
            return@withContext SongResult(false, error = "AudioRecord failed to initialize.")
        }

        // We will record 8 seconds of audio to build a solid signature
        val recordDurationSeconds = 8
        val bytesPerSample = 2 // 16-bit PCM = 2 bytes
        val totalBytesToRead = sampleRate * bytesPerSample * recordDurationSeconds
        val destination = ByteBuffer.allocate(totalBytesToRead)

        // Elevate thread priority for stable audio recording
        Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_AUDIO)

        try {
            audioRecord.startRecording()
            val readBuffer = ByteArray(bufferSize)

            // 4. Read audio and feed it into the buffer
            while (destination.remaining() > 0) {
                val actualRead = audioRecord.read(readBuffer, 0, bufferSize)
                if (actualRead < 0) {
                    throw Exception("AudioRecord error code: $actualRead")
                }

                val byteArray = if (actualRead < bufferSize) {
                    readBuffer.copyOfRange(0, actualRead)
                } else {
                    readBuffer
                }

                if (byteArray.size <= destination.remaining()) {
                    destination.put(byteArray)
                } else {
                    destination.put(byteArray, 0, destination.remaining())
                }

                // Append chunks to ShazamKit
                signatureGenerator.append(byteArray, actualRead, System.currentTimeMillis())
            }

            // 5. Generate fingerprint and query Shazam
            val signature = signatureGenerator.generateSignature()

            val sessionResult = ShazamKit.createSession(catalog)
            if (sessionResult is ShazamKitResult.Failure<*>) {
                return@withContext SongResult(false, error = "Failed to create matching session.")
            }
            val session = (sessionResult as ShazamKitResult.Success<*>).data as com.shazam.shazamkit.Session

            // 6. Handle the Result
            return@withContext when (val matchResult = session.match(signature)) {
                is MatchResult.Match -> {
                    // Usually the first item is the most confident match
                    val topMatch = matchResult.matchedMediaItems.firstOrNull()
                    if (topMatch != null) {
                        SongResult(true, topMatch.title ?: "Unknown", topMatch.artist ?: "Unknown")
                    } else {
                        SongResult(false, error = "Match found, but media data was empty.")
                    }
                }
                is MatchResult.NoMatch -> {
                    SongResult(false, error = "No match found.")
                }
                is MatchResult.Error -> {
                    SongResult(false, error = "Shazam API Error: ${matchResult.exception.message}")
                }
            }

        } catch (e: Exception) {
            return@withContext SongResult(false, error = "Exception: ${e.localizedMessage}")
        } finally {
            // ALWAYS release resources
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
