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
import java.util.UUID

class TrebleService : Service() {

    private val appUuid = UUID.fromString("c49abd69-dd2c-4655-a7ce-ec7da67aa930")
    private var dataReceiver: BroadcastReceiver? = null
    private val channelId = "treble_service_channel"

    override fun onCreate() {
        super.onCreate()

        // 1. Create the Notification Channel
        createNotificationChannel()

        // 2. Build the Notification
        val notification: Notification = NotificationCompat.Builder(this, channelId)
            .setContentTitle("Treble is Listening")
            .setContentText("Ready for watch commands")
            .setSmallIcon(android.R.drawable.ic_btn_speak_now)
            .setOngoing(true)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .build()

        // 3. Start Foreground
        val foregroundType = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE
        } else {
            0
        }

        try {
            ServiceCompat.startForeground(
                this,
                1,
                notification,
                foregroundType
            )
        } catch (e: Exception) {
            e.printStackTrace()
        }

        // 4. Register the Pebble Broadcast Receiver
        dataReceiver = object : PebbleKit.PebbleDataReceiver(appUuid) {
            override fun receiveData(context: Context, transactionId: Int, dict: PebbleDictionary) {
                PebbleKit.sendAckToPebble(context, transactionId)

                // KEY_COMMAND = 0
                val commandValue = dict.getInteger(0)
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
    }

    private fun handleRecognitionRequest() {
        try {
            // TODO: Start actual Microphone / Recognition logic here.
            sendRecognitionResult(isSuccess = true)
        } catch (e: SecurityException) {
            sendRecognitionResult(isSuccess = false)
        }
    }

    private fun sendRecognitionResult(isSuccess: Boolean) {
        val dict = PebbleDictionary()
        if (isSuccess) {
            dict.addInt32(1, 1) // KEY_RESPONSE_RESULT = RES_SUCCESS
            dict.addString(2, "Sandstorm") // KEY_SONG_TITLE
            dict.addString(3, "Darude") // KEY_SONG_ARTIST
        } else {
            dict.addInt32(1, 0) // KEY_RESPONSE_RESULT = RES_FAILED
        }
        PebbleKit.sendDataToPebble(this, appUuid, dict)
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        return START_STICKY
    }

    override fun onDestroy() {
        super.onDestroy()
        dataReceiver?.let { unregisterReceiver(it) }
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
