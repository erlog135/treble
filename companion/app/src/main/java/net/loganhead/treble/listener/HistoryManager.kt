package net.loganhead.treble.listener

import android.content.Context
import androidx.core.content.edit
import com.google.gson.Gson
import com.google.gson.reflect.TypeToken

class HistoryManager(context: Context) {
    private val prefs = context.getSharedPreferences("treble_history", Context.MODE_PRIVATE)
    private val gson = Gson()

    fun getHistory(): List<HistoryEntry> {
        val json = prefs.getString("history_list", null) ?: return emptyList()
        return try {
            val type = object : TypeToken<List<HistoryEntry>>() {}.type
            gson.fromJson(json, type)
        } catch (e: Exception) {
            emptyList()
        }
    }

    fun addEntry(entry: HistoryEntry) {
        val history = getHistory().toMutableList()
        history.add(0, entry)
        // Keep only the last 100 entries
        val limitedHistory = history.take(100)
        saveHistory(limitedHistory)
    }

    private fun saveHistory(history: List<HistoryEntry>) {
        val json = gson.toJson(history)
        prefs.edit {
            putString("history_list", json)
        }
    }
}
