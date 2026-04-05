package net.loganhead.treble.listener

import android.Manifest
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import com.getpebble.android.kit.PebbleKit
import net.loganhead.treble.listener.ui.theme.TrebleListenerTheme
import java.util.UUID

class MainActivity : ComponentActivity() {

    private val appUuid = UUID.fromString("c49abd69-dd2c-4655-a7ce-ec7da67aa930")
    private val logMessages = mutableStateListOf<String>()

    // Broadcast receiver to get logs from the TrebleService
    private val logReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            val message = intent.getStringExtra("message")
            if (message != null) {
                logMessages.add(message)
            }
        }
    }

    private val requestPermissionsLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        val micGranted = permissions[Manifest.permission.RECORD_AUDIO] ?: false
        if (micGranted) {
            logMessages.add("Permissions granted. Starting Service...")
            startTrebleService()
        } else {
            logMessages.add("Error: Mic permission denied. Service cannot run.")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        logMessages.add("Checking permissions...")

        val shazamManager = ShazamManager(this)

        // Define required permissions (Notification permission needed for Android 13+)
        val requiredPermissions = mutableListOf(Manifest.permission.RECORD_AUDIO)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            requiredPermissions.add(Manifest.permission.POST_NOTIFICATIONS)
        }

        if (!shazamManager.hasMicrophonePermission()) {
            requestPermissionsLauncher.launch(requiredPermissions.toTypedArray())
        } else {
            startTrebleService()
        }

        setContent {
            TrebleListenerTheme {
                Scaffold(modifier = Modifier.fillMaxSize()) { innerPadding ->
                    TrebleScreen(
                        logMessages = logMessages,
                        onLaunchClicked = { launchWatchApp() },
                        onSimulateClicked = { forceServiceToListen() },
                        modifier = Modifier.padding(innerPadding)
                    )
                }
            }
        }
    }

    override fun onResume() {
        super.onResume()
        // Register the log receiver so the UI updates while the app is open
        val filter = IntentFilter("net.loganhead.treble.LOG_EVENT")
        ContextCompat.registerReceiver(
            this,
            logReceiver,
            filter,
            ContextCompat.RECEIVER_NOT_EXPORTED // Securely listen only to our own app
        )
    }

    override fun onPause() {
        super.onPause()
        unregisterReceiver(logReceiver)
    }

    private fun startTrebleService() {
        val serviceIntent = Intent(this, TrebleService::class.java)
        ContextCompat.startForegroundService(this, serviceIntent)
    }

    private fun launchWatchApp() {
        PebbleKit.startAppOnPebble(this, appUuid)
        logMessages.add("Command sent to open watchapp.")
    }

    private fun forceServiceToListen() {
        val intent = Intent(this, TrebleService::class.java).apply {
            action = "ACTION_FORCE_RECOGNIZE"
        }
        startService(intent)
    }
}

@Composable
fun TrebleScreen(
    logMessages: List<String>,
    onLaunchClicked: () -> Unit,
    onSimulateClicked: () -> Unit,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier.fillMaxSize().padding(16.dp)
    ) {

        Button(
            onClick = onLaunchClicked,
            modifier = Modifier.fillMaxWidth()
        ) {
            Text("Open App on Watch")
        }

        Spacer(modifier = Modifier.height(8.dp))
        Button(
            onClick = onSimulateClicked,
            modifier = Modifier.fillMaxWidth()
        ) {
            Text("Force Service to Listen (Test)")
        }

        Spacer(modifier = Modifier.height(16.dp))

        Text(text = "Activity Log:", modifier = Modifier.padding(bottom = 8.dp))

        LazyColumn(modifier = Modifier.fillMaxSize()) {
            items(logMessages.reversed()) { message ->
                Text(text = message, modifier = Modifier.padding(vertical = 4.dp))
            }
        }
    }
}