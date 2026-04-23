package net.loganhead.treble.listener

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

@Composable
fun CompanionScreen(
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
