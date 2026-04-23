package net.loganhead.treble.listener

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Mic
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

@Composable
fun ListenScreen(
    onListenClicked: () -> Unit,
    modifier: Modifier = Modifier
) {
    Box(
        modifier = modifier.fillMaxSize(),
        contentAlignment = Alignment.Center
    ) {
        Column(
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Button(
                onClick = onListenClicked,
                modifier = Modifier.size(200.dp),
                shape = CircleShape,
                elevation = ButtonDefaults.buttonElevation(defaultElevation = 8.dp)
            ) {
                Icon(
                    imageVector = Icons.Default.Mic,
                    contentDescription = "Listen",
                    modifier = Modifier.size(80.dp)
                )
            }
            Spacer(modifier = Modifier.height(24.dp))
            Text(
                text = "Tap to Listen",
                style = MaterialTheme.typography.headlineSmall
            )
        }
    }
}
