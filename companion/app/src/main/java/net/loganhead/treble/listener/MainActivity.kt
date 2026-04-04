package net.loganhead.treble.listener

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
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
import com.getpebble.android.kit.Constants
import com.getpebble.android.kit.util.PebbleDictionary
import net.loganhead.treble.listener.ui.theme.TrebleListenerTheme
import java.util.UUID

class MainActivity : ComponentActivity() {

    private val appUuid = UUID.fromString("c49abd69-dd2c-4655-a7ce-ec7da67aa930")

    // Keys matching package.json
    private val KEY_COMMAND = 0
    private val KEY_RESPONSE_RESULT = 1
    private val KEY_SONG_TITLE = 2
    private val KEY_SONG_ARTIST = 3

    // Commands & Results
    private val CMD_START_RECOGNITION = 1
    private val RES_SUCCESS = 1
    private val RES_FAILED = 0

    private val logMessages = mutableStateListOf<String>()
    private var dataReceiver: BroadcastReceiver? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()

        logMessages.add("Ready. Press 'Select' on the watch to recognize music.")

        val serviceIntent = Intent(this, TrebleService::class.java)
        ContextCompat.startForegroundService(this, serviceIntent)
        
        setContent {
            TrebleListenerTheme {
                Scaffold(modifier = Modifier.fillMaxSize()) { innerPadding ->
                    TrebleScreen(
                        logMessages = logMessages,
                        onSimulateClicked = { sendRecognitionResult(true) }, // For manual testing
                        onLaunchClicked = { launchWatchApp() },
                        modifier = Modifier.padding(innerPadding)
                    )
                }
            }
        }
    }

    private fun launchWatchApp() {

        // Sends the command to the Pebble app on your phone to open your watchapp
        PebbleKit.startAppOnPebble(this, appUuid)
        logMessages.add("Command sent to open watchapp.")
    }

    private fun sendRecognitionResult(isSuccess: Boolean) {

        val dict = PebbleDictionary()

        if (isSuccess) {
            dict.addInt32(KEY_RESPONSE_RESULT, RES_SUCCESS)
            dict.addString(KEY_SONG_TITLE, "Sandstorm")
            dict.addString(KEY_SONG_ARTIST, "Darude")
            logMessages.add("Sent result: Success (Sandstorm - Darude)")
        } else {
            dict.addInt32(KEY_RESPONSE_RESULT, RES_FAILED)
            logMessages.add("Sent result: Failed to recognize.")
        }

        PebbleKit.sendDataToPebble(this, appUuid, dict)
    }

    override fun onResume() {
        super.onResume()
        dataReceiver = object : PebbleKit.PebbleDataReceiver(appUuid) {
            override fun receiveData(context: Context, transactionId: Int, dict: PebbleDictionary) {
                logMessages.add("Received data from watch.")
                // ALWAYS ACK the message immediately
                PebbleKit.sendAckToPebble(context, transactionId)

                // Check if the watch sent a command
                val commandValue = dict.getInteger(KEY_COMMAND)
                if (commandValue != null) {
                    when (commandValue.toInt()) {
                        CMD_START_RECOGNITION -> {
                            logMessages.add("Watch requested music recognition. Processing...")

                            // TODO: Call your actual microphone/recognition API here.
                            // For now, we simulate a successful hit immediately:
                            sendRecognitionResult(true)
                        }
                        else -> logMessages.add("Received unknown command: $commandValue")
                    }
                }
            }
        }
        
        // Manual registration to support Android 14+ (RECEIVER_EXPORTED)
        // because PebbleKit 4.0.1 does not specify it internally.
        val filter = IntentFilter(Constants.INTENT_APP_RECEIVE)

        ContextCompat.registerReceiver(
            this,
            dataReceiver,
            filter,
            ContextCompat.RECEIVER_EXPORTED
        )
    }

    override fun onPause() {
        super.onPause()
        dataReceiver?.let { unregisterReceiver(it) }
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
            Text("Simulate Successful Recognition")
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