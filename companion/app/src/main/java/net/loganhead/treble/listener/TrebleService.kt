package net.loganhead.treble.listener

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.Service
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.content.pm.ServiceInfo
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.os.Build
import android.os.IBinder
import android.os.PowerManager
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

    // Message Keys
    private val KEY_COMMAND = 0
    private val KEY_RESPONSE_RESULT = 1
    private val KEY_SONG_TITLE = 2
    private val KEY_SONG_ARTIST = 3

    // Commands
    private val CMD_START_RECOGNITION = 1
    private val CMD_CHECK_READY = 2

    // Response Result Values
    private val RES_SUCCESS = 0
    private val RES_FAILED = 1
    private val RES_NO_APP = 2     // Service/Internet unavailable
    private val RES_NO_PERMS = 3   // Permissions missing

    // Scope for background Shazam requests
    private val serviceScope = CoroutineScope(Dispatchers.Main + SupervisorJob())
    private lateinit var shazamManager: ShazamManager

    override fun onCreate() {
        super.onCreate()

        shazamManager = ShazamManager(this)

        // 1. Create the Notification Channel & Notification
        createNotificationChannel()
        val notification: Notification = NotificationCompat.Builder(this, channelId)
            .setContentTitle("Treble Listener")
            .setContentText("Awaiting commands from watchapp")
            .setSmallIcon(R.drawable.ic_listener_notification)
            .setOngoing(true)
            .setShowWhen(false)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .build()

        // 2. Start Foreground
        // On Android 14+, we can only use FOREGROUND_SERVICE_TYPE_MICROPHONE if we have the permission.
        // If not, we start with 0 to at least keep the service alive to respond with "No Permissions".
        val hasMicPermission = ContextCompat.checkSelfPermission(this, android.Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED
        
        val foregroundType = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R && hasMicPermission) {
            ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE
        } else {
            0
        }

        try {
            ServiceCompat.startForeground(this, 1, notification, foregroundType)
        } catch (e: Exception) {
            // Fallback for strict enforcement on newer Android versions
            try {
                ServiceCompat.startForeground(this, 1, notification, 0)
            } catch (fallbackEx: Exception) {
                fallbackEx.printStackTrace()
            }
        }

        // 3. Register the Pebble Broadcast Receiver
        dataReceiver = object : PebbleKit.PebbleDataReceiver(appUuid) {
            override fun receiveData(context: Context, transactionId: Int, dict: PebbleDictionary) {
                PebbleKit.sendAckToPebble(context, transactionId)

                val commandValue = dict.getInteger(KEY_COMMAND)
                if (commandValue != null) {
                    when (commandValue.toInt()) {
                        CMD_START_RECOGNITION -> handleRecognitionRequest()
                        CMD_CHECK_READY -> sendReadyStatus()
                    }
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
        // Triggered by the "Force Service to Listen" button in the UI or by Boot/Activity
        if (intent?.action == "ACTION_FORCE_RECOGNIZE") {
            handleRecognitionRequest()
        }
        
        // Ensure foreground state is maintained if restarted
        return START_STICKY
    }

    private fun handleRecognitionRequest() {
        val readiness = checkReadiness()
        if (readiness != RES_SUCCESS) {
            sendLogToActivity("Recognition blocked: Readiness status $readiness")
            sendResponse(readiness)
            return
        }

        sendLogToActivity("Recognition requested. Listening...")

        serviceScope.launch {
            val result = shazamManager.recognizeMusic()
            if (result.isSuccess) {
                sendLogToActivity("Found: ${result.title} by ${result.artist}")
                sendRecognitionResult(result.title, result.artist)
            } else {
                sendLogToActivity("Failed: ${result.error}")
                sendResponse(RES_FAILED)
            }
        }
    }

    private fun checkReadiness(): Int {
        // 1. Microphone Permission
        if (ContextCompat.checkSelfPermission(this, android.Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            return RES_NO_PERMS
        }

        // 2. Notification Permission (Android 13+)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (ContextCompat.checkSelfPermission(this, android.Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                return RES_NO_PERMS
            }
        }

        // 3. Battery Optimization
        val powerManager = getSystemService(Context.POWER_SERVICE) as PowerManager
        if (!powerManager.isIgnoringBatteryOptimizations(packageName)) {
            return RES_NO_PERMS
        }

        // 4. Internet Availability (Shazam requires cloud)
        if (!isNetworkAvailable(this)) {
            return RES_NO_APP
        }
        
        return RES_SUCCESS
    }

    private fun isNetworkAvailable(context: Context): Boolean {
        val connectivityManager = context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
        val network = connectivityManager.activeNetwork ?: return false
        val activeNetwork = connectivityManager.getNetworkCapabilities(network) ?: return false
        return when {
            activeNetwork.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) -> true
            activeNetwork.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR) -> true
            activeNetwork.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET) -> true
            else -> false
        }
    }

    private fun sendReadyStatus() {
        val status = checkReadiness()
        sendResponse(status)
        sendLogToActivity("Sent readiness status: $status")
    }

    private fun sendResponse(resultCode: Int) {
        val dict = PebbleDictionary()
        dict.addInt32(KEY_RESPONSE_RESULT, resultCode)
        PebbleKit.sendDataToPebble(this, appUuid, dict)
    }

    private fun sendRecognitionResult(title: String, artist: String) {
        val dict = PebbleDictionary()
        dict.addInt32(KEY_RESPONSE_RESULT, RES_SUCCESS)
        dict.addString(KEY_SONG_TITLE, title)
        dict.addString(KEY_SONG_ARTIST, artist)
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
