package net.loganhead.treble.listener

import android.Manifest
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.PowerManager
import android.provider.Settings
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.History
import androidx.compose.material.icons.filled.Mic
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import androidx.core.content.ContextCompat
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import androidx.lifecycle.compose.LocalLifecycleOwner
import com.getpebble.android.kit.PebbleKit
import net.loganhead.treble.listener.ui.theme.TrebleListenerTheme
import java.util.UUID

class MainActivity : ComponentActivity() {

    private val appUuid = UUID.fromString("c49abd69-dd2c-4655-a7ce-ec7da67aa930")
    private val logMessages = mutableStateListOf<String>()
    private val history = mutableStateListOf<HistoryEntry>()
    private lateinit var historyManager: HistoryManager

    private val logReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            when (intent.action) {
                "net.loganhead.treble.LOG_EVENT" -> {
                    intent.getStringExtra("message")?.let { logMessages.add(it) }
                }
                "net.loganhead.treble.SONG_DETECTED" -> {
                    val title = intent.getStringExtra("title") ?: "Unknown"
                    val artist = intent.getStringExtra("artist") ?: "Unknown"
                    val source = intent.getStringExtra("source") ?: "App"
                    val timestamp = intent.getLongExtra("timestamp", System.currentTimeMillis())
                    
                    val entry = HistoryEntry(title, artist, timestamp, source)
                    if (!history.contains(entry)) {
                        history.add(0, entry)
                    }
                }
            }
        }
    }

    private val requestPermissionsLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { _ ->
        startTrebleService()
    }

    @OptIn(ExperimentalMaterial3Api::class)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        historyManager = HistoryManager(this)
        loadHistory()
        startTrebleService()

        setContent {
            TrebleListenerTheme {
                val context = LocalContext.current
                val lifecycleOwner = LocalLifecycleOwner.current
                var showPermissionsPage by remember { mutableStateOf(false) }
                var selectedTab by remember { mutableIntStateOf(0) }
                
                var refreshTrigger by remember { mutableIntStateOf(0) }
                DisposableEffect(lifecycleOwner) {
                    val observer = LifecycleEventObserver { _, event ->
                        if (event == Lifecycle.Event.ON_RESUME) {
                            refreshTrigger++
                            loadHistory()
                        }
                    }
                    lifecycleOwner.lifecycle.addObserver(observer)
                    onDispose { lifecycleOwner.lifecycle.removeObserver(observer) }
                }

                val permissionsStatus = remember(refreshTrigger) { checkAllPermissions(context) }

                LaunchedEffect(refreshTrigger) {
                    startTrebleService()
                }

                Scaffold(
                    modifier = Modifier.fillMaxSize(),
                    topBar = {
                        TopAppBar(
                            title = { Text("Treble") },
                            actions = {
                                Button(
                                    onClick = { showPermissionsPage = !showPermissionsPage },
                                    colors = if (permissionsStatus) {
                                        ButtonDefaults.buttonColors(
                                            containerColor = Color(0xFF2E7D32),
                                            contentColor = Color.White
                                        )
                                    } else {
                                        ButtonDefaults.buttonColors(
                                            containerColor = MaterialTheme.colorScheme.error,
                                            contentColor = MaterialTheme.colorScheme.onError
                                        )
                                    },
                                    contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp),
                                    modifier = Modifier.padding(end = 8.dp)
                                ) {
                                    Icon(
                                        imageVector = if (permissionsStatus) Icons.Default.CheckCircle else Icons.Default.Warning,
                                        contentDescription = null,
                                        modifier = Modifier.size(18.dp)
                                    )
                                    Spacer(Modifier.width(8.dp))
                                    Text(
                                        text = if (permissionsStatus) "Ready" else "Fix Needed",
                                        style = MaterialTheme.typography.labelLarge
                                    )
                                }
                            }
                        )
                    },
                    bottomBar = {
                        if (!showPermissionsPage) {
                            NavigationBar {
                                NavigationBarItem(
                                    selected = selectedTab == 0,
                                    onClick = { selectedTab = 0 },
                                    icon = { Icon(Icons.Default.Mic, contentDescription = "Listen") },
                                    label = { Text("Listen") }
                                )
                                NavigationBarItem(
                                    selected = selectedTab == 1,
                                    onClick = { selectedTab = 1 },
                                    icon = { Icon(Icons.Default.History, contentDescription = "History") },
                                    label = { Text("History") }
                                )
                                NavigationBarItem(
                                    selected = selectedTab == 2,
                                    onClick = { selectedTab = 2 },
                                    icon = { Icon(Icons.Default.Settings, contentDescription = "Companion") },
                                    label = { Text("Companion") }
                                )
                            }
                        }
                    }
                ) { innerPadding ->
                    if (showPermissionsPage) {
                        PermissionsScreen(
                            onClose = { 
                                showPermissionsPage = false
                                startTrebleService()
                            },
                            onRequestMic = { requestPermissionsLauncher.launch(arrayOf(Manifest.permission.RECORD_AUDIO)) },
                            onRequestNotifications = {
                                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                                    requestPermissionsLauncher.launch(arrayOf(Manifest.permission.POST_NOTIFICATIONS))
                                }
                            },
                            onRequestBattery = {
                                val intent = Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS).apply {
                                    data = Uri.parse("package:$packageName")
                                }
                                startActivity(intent)
                            },
                            modifier = Modifier.padding(innerPadding)
                        )
                    } else {
                        when (selectedTab) {
                            0 -> ListenScreen(
                                onListenClicked = { forceServiceToListen() },
                                modifier = Modifier.padding(innerPadding)
                            )
                            1 -> HistoryScreen(
                                history = history,
                                modifier = Modifier.padding(innerPadding)
                            )
                            2 -> CompanionScreen(
                                logMessages = logMessages,
                                onLaunchClicked = { launchWatchApp() },
                                onSimulateClicked = { forceServiceToListen() },
                                modifier = Modifier.padding(innerPadding)
                            )
                        }
                    }
                }
            }
        }
    }

    private fun loadHistory() {
        val savedHistory = historyManager.getHistory()
        // Simple sync: if sizes differ or we want to be sure, refresh.
        // For a better implementation, we'd use a Flow or LiveData.
        if (savedHistory.size != history.size) {
            history.clear()
            history.addAll(savedHistory)
        }
    }

    override fun onResume() {
        super.onResume()
        val filter = IntentFilter().apply {
            addAction("net.loganhead.treble.LOG_EVENT")
            addAction("net.loganhead.treble.SONG_DETECTED")
        }
        ContextCompat.registerReceiver(
            this,
            logReceiver,
            filter,
            ContextCompat.RECEIVER_NOT_EXPORTED
        )
    }

    override fun onPause() {
        super.onPause()
        unregisterReceiver(logReceiver)
    }

    private fun checkAllPermissions(context: Context): Boolean {
        val mic = ContextCompat.checkSelfPermission(context, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED
        val notifications = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            ContextCompat.checkSelfPermission(context, Manifest.permission.POST_NOTIFICATIONS) == PackageManager.PERMISSION_GRANTED
        } else true
        
        val powerManager = context.getSystemService(Context.POWER_SERVICE) as PowerManager
        val battery = powerManager.isIgnoringBatteryOptimizations(context.packageName)

        return mic && notifications && battery
    }

    private fun startTrebleService() {
        val serviceIntent = Intent(this, TrebleService::class.java)
        try {
            ContextCompat.startForegroundService(this, serviceIntent)
        } catch (e: Exception) {
            e.printStackTrace()
        }
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
