package net.loganhead.treble.listener

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.os.Build
import android.os.PowerManager
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat

@Composable
fun PermissionsScreen(
    onClose: () -> Unit,
    onRequestMic: () -> Unit,
    onRequestNotifications: () -> Unit,
    onRequestBattery: () -> Unit,
    modifier: Modifier = Modifier
) {
    val context = LocalContext.current
    
    val micGranted = ContextCompat.checkSelfPermission(context, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED
    val notifGranted = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
        ContextCompat.checkSelfPermission(context, Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED
    } else true
    
    val powerManager = context.getSystemService(Context.POWER_SERVICE) as PowerManager
    val batteryIgnored = powerManager.isIgnoringBatteryOptimizations(context.packageName)
    
    val isInternetAvailable = isNetworkAvailable(context)

    val allReady = micGranted && notifGranted && batteryIgnored

    Column(
        modifier = modifier.fillMaxSize().padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        Text(
            text = "System Setup",
            style = MaterialTheme.typography.headlineMedium
        )
        Text(
            text = "Ensure all items are green for Treble to work correctly with your Pebble.",
            style = MaterialTheme.typography.bodyMedium
        )

        PermissionItem(
            title = "Microphone",
            description = "Required to identify music.",
            isGranted = micGranted,
            onGrant = onRequestMic
        )

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            PermissionItem(
                title = "Notifications",
                description = "Required for the background service.",
                isGranted = notifGranted,
                onGrant = onRequestNotifications
            )
        }

        PermissionItem(
            title = "Always Active",
            description = "Ignore battery optimizations to prevent the system from killing the listener.",
            isGranted = batteryIgnored,
            onGrant = onRequestBattery
        )

        PermissionItem(
            title = "Internet Connection",
            description = "Required for Shazam music identification.",
            isGranted = isInternetAvailable,
            onGrant = { /* No direct grant, just status */ },
            showButton = false
        )

        Spacer(modifier = Modifier.weight(1f))

        Button(
            onClick = onClose,
            modifier = Modifier.fillMaxWidth()
        ) {
            Text(if (allReady) "Return to Logs" else "Close (Service may not work)")
        }
    }
}

@Composable
fun PermissionItem(
    title: String,
    description: String,
    isGranted: Boolean,
    onGrant: () -> Unit,
    showButton: Boolean = true
) {
    val isDark = isSystemInDarkTheme()
    
    // Theme-aware colors for the status cards
    val containerColor = if (isGranted) {
        if (isDark) Color(0xFF1B5E20) else Color(0xFFE8F5E9)
    } else {
        if (isDark) Color(0xFF4E342E) else Color(0xFFFFF3E0)
    }
    val contentColor = if (isGranted) {
        if (isDark) Color(0xFFC8E6C9) else Color(0xFF2E7D32)
    } else {
        if (isDark) Color(0xFFFFCCBC) else Color(0xFFE65100)
    }

    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(
            containerColor = containerColor,
            contentColor = contentColor
        )
    ) {
        Row(
            modifier = Modifier.padding(16.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = title, 
                    style = MaterialTheme.typography.titleMedium,
                    color = contentColor
                )
                Text(
                    text = description, 
                    style = MaterialTheme.typography.bodySmall,
                    color = contentColor.copy(alpha = 0.8f)
                )
            }
            if (isGranted) {
                Icon(Icons.Default.CheckCircle, contentDescription = "Granted", tint = contentColor)
            } else if (showButton) {
                Button(
                    onClick = onGrant,
                    // Ensure button colors are explicitly using the theme's colors
                    colors = ButtonDefaults.buttonColors(
                        containerColor = MaterialTheme.colorScheme.primary,
                        contentColor = MaterialTheme.colorScheme.onPrimary
                    )
                ) {
                    Text("Enable")
                }
            } else {
                Icon(Icons.Default.Warning, contentDescription = "Required", tint = contentColor)
            }
        }
    }
}

fun isNetworkAvailable(context: Context): Boolean {
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
