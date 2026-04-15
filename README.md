# Treble

Treble is a music recognition app for Pebble smartwatches, powered by modern Android architecture and Apple's ShazamKit API. 

It allows you to use your Pebble to instantly start listening for music via your phone's microphone, silently pinging the Shazam API in the background, and returning the song title and artist directly to your watch.

## Features
* **Background Processing**: Runs as an Android Foreground Service, meaning it works even when your phone is locked or in your pocket.
* **Continuous Audio Streaming**: Utilizes ShazamKit's `StreamingSession` and Android's internal noise reduction to quickly and accurately match songs in noisy environments.
* **Two-Way Communication**: Uses PebbleKit Android to seamlessly pass commands and dictionaries between the C watchapp and the modern Kotlin service.
* **Private**: Microphone access is only triggered when you begin listening from the watchapp (or use the test function on the companion app). The sound is irreversibly converted into an audio signature before leaving the device for matching, so no raw audio is ever sent to anyone. According to [Apple](https://developer.apple.com/shazamkit/), "Audio is not shared with Apple and audio signatures cannot be inverted, ensuring content remains secure and private."

## Download
* The latest verison of both apps are available to download at the [releases page](https://github.com/erlog135/treble/releases).

