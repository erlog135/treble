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
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.filled.Warning
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalLifecycleOwner
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import com.getpebble.android.kit.PebbleKit
import net.loganhead.treble.listener.ui.theme.TrebleListenerTheme
import java.util.UUID

class MainActivity : ComponentActivity() {

    private val appUuid = UUID.fromString("c49abd69-dd2c-4655-a7ce-ec7da67aa930")
    private val logMessages = mutableStateListOf<String>()

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
    ) { _ ->
        // Ensure service refreshes its foreground state with new permissions
        startTrebleService()
    }

    @OptIn(ExperimentalMaterial3Api::class)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        // Initial service start
        startTrebleService()

        setContent {
            TrebleListenerTheme {
                val context = LocalContext.current
                val lifecycleOwner = LocalLifecycleOwner.current
                var showPermissionsPage by remember { mutableStateOf(false) }
                
                // Track permission status reactively with lifecycle awareness
                var refreshTrigger by remember { mutableIntStateOf(0) }
                DisposableEffect(lifecycleOwner) {
                    val observer = LifecycleEventObserver { _, event ->
                        if (event == Lifecycle.Event.ON_RESUME) {
                            refreshTrigger++
                        }
                    }
                    lifecycleOwner.lifecycle.addObserver(observer)
                    onDispose { lifecycleOwner.lifecycle.removeObserver(observer) }
                }

                val permissionsStatus = remember(refreshTrigger) { checkAllPermissions(context) }

                // Poke service whenever we return to the app or permissions change
                // This ensures the foreground notification appears immediately after grants
                LaunchedEffect(refreshTrigger) {
                    startTrebleService()
                }

                Scaffold(
                    modifier = Modifier.fillMaxSize(),
                    topBar = {
                        TopAppBar(
                            title = { Text("Treble Listener") },
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
                    }
                ) { innerPadding ->
                    if (showPermissionsPage) {
                        PermissionsScreen(
                            onClose = { 
                                showPermissionsPage = false
                                startTrebleService() // Poke service again when closing permissions
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
    }

    override fun onResume() {
        super.onResume()
        val filter = IntentFilter("net.loganhead.treble.LOG_EVENT")
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

        Spacer(modifier = Modifier.height(24.dp))

        Text(
            text = "Activity Log",
            style = MaterialTheme.typography.titleMedium,
            modifier = Modifier.padding(bottom = 8.dp)
        )

        val listState = rememberLazyListState()
        
        // Auto-scroll to bottom when new messages arrive
        LaunchedEffect(logMessages.size) {
            if (logMessages.isNotEmpty()) {
                listState.animateScrollToItem(logMessages.size - 1)
            }
        }

        Box(
            modifier = Modifier
                .fillMaxSize()
                .clip(RoundedCornerShape(8.dp))
                .background(MaterialTheme.colorScheme.surfaceVariant.copy(alpha = 0.3f))
                .border(1.dp, MaterialTheme.colorScheme.outlineVariant, RoundedCornerShape(8.dp))
                .padding(8.dp)
        ) {
            LazyColumn(
                state = listState,
                modifier = Modifier.fillMaxSize()
            ) {
                items(logMessages) { message ->
                    Text(
                        text = message,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                        fontFamily = FontFamily.Monospace,
                        fontSize = 13.sp,
                        lineHeight = 18.sp,
                        modifier = Modifier.padding(vertical = 2.dp)
                    )
                }
            }
        }
    }
}
