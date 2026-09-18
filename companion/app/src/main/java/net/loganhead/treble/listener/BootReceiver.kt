package net.loganhead.treble.listener

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.os.Build
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat

class BootReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action == Intent.ACTION_BOOT_COMPLETED || 
            intent.action == "android.intent.action.QUICKBOOT_POWERON") {
            
            // 1. Always post the notification to the user upon boot
            showBootNotification(context)

            // 2. Attempt to start the service automatically
            val serviceIntent = Intent(context, TrebleService::class.java)
            try {
                ContextCompat.startForegroundService(context, serviceIntent)
            } catch (e: Exception) {
                // Background start may fail on some Android versions/conditions
                e.printStackTrace()
            }
        }
    }

    private fun showBootNotification(context: Context) {
        val notificationManager = context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        val channelId = "treble_boot_alerts"

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                channelId,
                "Treble Boot Alerts",
                NotificationManager.IMPORTANCE_HIGH
            ).apply {
                description = "Notifies the user to re-open the app after a device reboot"
            }
            notificationManager.createNotificationChannel(channel)
        }

        val activityIntent = Intent(context, MainActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
        }
        
        val pendingIntent = PendingIntent.getActivity(
            context, 
            0, 
            activityIntent, 
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
        )

        val notification = NotificationCompat.Builder(context, channelId)
            .setSmallIcon(R.drawable.ic_listener_notification)
            .setContentTitle("Treble Listener")
            .setContentText("Restart detected. Open the app to re-enable background access.")
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .setCategory(NotificationCompat.CATEGORY_REMINDER)
            .setAutoCancel(true)
            .setContentIntent(pendingIntent)
            .build()

        notificationManager.notify(1001, notification)
    }
}
