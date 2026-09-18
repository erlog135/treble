package net.loganhead.treble.listener

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.provider.Settings
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat

class BootReceiver : BroadcastReceiver() {

    companion object {
        private const val CHANNEL_ID = "treble_boot_alerts"
        private const val NOTIFICATION_ID = 1001
    }

    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action != Intent.ACTION_BOOT_COMPLETED &&
            intent.action != "android.intent.action.QUICKBOOT_POWERON") return

        // Don't start the background service here. The service is started by
        // MainActivity on onCreate/onResume, which ensures mic access is fully
        // established before the persistent "Ready" notification appears.
        // Just notify the user that they need to open the app.
        if (canPostNotifications(context)) {
            showBootNotification(context)
        } else {
            // POST_NOTIFICATIONS not granted – fall back to a notification-settings
            // deeplink so the user can unblock notifications themselves.
            showNotificationPermissionReminder(context)
        }
    }

    // -------------------------------------------------------------------------
    // Notification helpers
    // -------------------------------------------------------------------------

    private fun canPostNotifications(context: Context): Boolean {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            return ContextCompat.checkSelfPermission(
                context,
                android.Manifest.permission.POST_NOTIFICATIONS
            ) == PackageManager.PERMISSION_GRANTED
        }
        return true // permission not required below API 33
    }

    private fun ensureChannel(notificationManager: NotificationManager) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "Treble Boot Alerts",
                NotificationManager.IMPORTANCE_HIGH
            ).apply {
                description = "Notifies the user to re-open the app after a device reboot"
            }
            notificationManager.createNotificationChannel(channel)
        }
    }

    /**
     * Shown when POST_NOTIFICATIONS is granted.
     * Always asks the user to reopen the app — mic access requires a foreground
     * UI session even when the background service has already started.
     */
    private fun showBootNotification(context: Context) {
        val nm = context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        ensureChannel(nm)

        val launchIntent = Intent(context, MainActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
        }
        val pendingIntent = PendingIntent.getActivity(
            context, 0, launchIntent,
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
        )

        val body = "Restart detected. Open the app to re-enable background access."

        val notification = NotificationCompat.Builder(context, CHANNEL_ID)
            .setSmallIcon(R.drawable.ic_listener_notification)
            .setContentTitle("Treble Listener")
            .setContentText(body)
            .setStyle(NotificationCompat.BigTextStyle().bigText(body))
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .setCategory(NotificationCompat.CATEGORY_REMINDER)
            .setAutoCancel(true)
            .setContentIntent(pendingIntent)
            .build()

        nm.notify(NOTIFICATION_ID, notification)
    }

    /**
     * Fallback when POST_NOTIFICATIONS has not been granted.
     * Opens the app's notification settings so the user can unblock it,
     * which will also prompt them to reopen the app.
     */
    private fun showNotificationPermissionReminder(context: Context) {
        val nm = context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        // We can still create the channel; it won't be visible until permission is granted
        ensureChannel(nm)

        // Deep-link to App Info or Notification Settings so the user can act
        val settingsIntent = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS).apply {
                putExtra(Settings.EXTRA_APP_PACKAGE, context.packageName)
                flags = Intent.FLAG_ACTIVITY_NEW_TASK
            }
        } else {
            Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS).apply {
                data = Uri.fromParts("package", context.packageName, null)
                flags = Intent.FLAG_ACTIVITY_NEW_TASK
            }
        }

        // As a last resort, try to start the settings activity directly.
        // This will only work if the system allows it (some OEMs block it from receivers),
        // but it's the best we can do without the notification permission.
        try {
            context.startActivity(settingsIntent)
        } catch (_: Exception) { /* nothing more we can do */ }
    }
}
