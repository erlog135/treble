package net.loganhead.treble.listener

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat
import androidx.core.app.ServiceCompat
import androidx.core.content.ContextCompat
import com.getpebble.android.kit.Constants
import com.getpebble.android.kit.PebbleKit
import com.getpebble.android.kit.util.PebbleDictionary
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import java.util.UUID

class TrebleService : Service() {

    private val appUuid = UUID.fromString("c49abd69-dd2c-4655-a7ce-ec7da67aa930")
    private var dataReceiver: BroadcastReceiver? = null
    private val channelId = "treble_service_channel"

    // Scope for background Shazam requests
    private val serviceScope = CoroutineScope(Dispatchers.Main + SupervisorJob())
    private lateinit var shazamManager: ShazamManager

    override fun onCreate() {
        super.onCreate()

        shazamManager = ShazamManager(this)

        // 1. Create the Notification Channel & Notification
        createNotificationChannel()
        val notification: Notification = NotificationCompat.Builder(this, channelId)
            .setContentTitle("Treble is Listening")
            .setContentText("Ready for watch commands")
            .setSmallIcon(android.R.drawable.ic_btn_speak_now)
            .setOngoing(true)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .build()

        // 2. Start Foreground
        val foregroundType = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE
        } else {
            0
        }

        try {
            ServiceCompat.startForeground(this, 1, notification, foregroundType)
        } catch (e: Exception) {
            e.printStackTrace()
        }

        // 3. Register the Pebble Broadcast Receiver
        dataReceiver = object : PebbleKit.PebbleDataReceiver(appUuid) {
            override fun receiveData(context: Context, transactionId: Int, dict: PebbleDictionary) {
                PebbleKit.sendAckToPebble(context, transactionId)

                val commandValue = dict.getInteger(0) // KEY_COMMAND
                if (commandValue != null && commandValue.toInt() == 1) { // CMD_START_RECOGNITION
                    handleRecognitionRequest()
                }
            }
        }

        val filter = IntentFilter(Constants.INTENT_APP_RECEIVE)
        ContextCompat.registerReceiver(
            this,
            dataReceiver,
            filter,
            ContextCompat.RECEIVER_EXPORTED
        )

        sendLogToActivity("Service started and listening to Pebble.")
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        // Triggered by the "Force Service to Listen" button in the UI
        if (intent?.action == "ACTION_FORCE_RECOGNIZE") {
            handleRecognitionRequest()
        }
        return START_STICKY
    }

    private fun handleRecognitionRequest() {
        sendLogToActivity("Recognition requested. Listening...")

        serviceScope.launch {
            val result = shazamManager.recognizeMusic()
            if (result.isSuccess) {
                sendLogToActivity("Found: ${result.title} by ${result.artist}")
                sendRecognitionResult(true, result.title, result.artist)
            } else {
                sendLogToActivity("Failed: ${result.error}")
                sendRecognitionResult(false)
            }
        }
    }

    private fun sendRecognitionResult(isSuccess: Boolean, title: String = "", artist: String = "") {
        val dict = PebbleDictionary()
        if (isSuccess) {
            dict.addInt32(1, 1) // KEY_RESPONSE_RESULT = RES_SUCCESS
            dict.addString(2, title) // KEY_SONG_TITLE
            dict.addString(3, artist) // KEY_SONG_ARTIST
        } else {
            dict.addInt32(1, 0) // KEY_RESPONSE_RESULT = RES_FAILED
        }
        PebbleKit.sendDataToPebble(this, appUuid, dict)
    }

    // Helper to pipe logs back to the MainActivity UI
    private fun sendLogToActivity(message: String) {
        val intent = Intent("net.loganhead.treble.LOG_EVENT")
        intent.putExtra("message", message)
        intent.setPackage(packageName)
        sendBroadcast(intent)
    }

    override fun onDestroy() {
        super.onDestroy()
        dataReceiver?.let { unregisterReceiver(it) }
        serviceScope.cancel() // Stop any running Shazam requests
    }

    override fun onBind(intent: Intent?): IBinder? = null

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                channelId,
                "Treble Background Service",
                NotificationManager.IMPORTANCE_LOW
            )
            val manager = getSystemService(NotificationManager::class.java)
            manager?.createNotificationChannel(channel)
        }
    }
}