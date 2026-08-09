package com.atuy.ykweditors

import android.content.Context
import android.net.Uri
import android.os.Bundle
import android.provider.OpenableColumns
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

private enum class GameKind(val nativeId: Int, val label: String) {
    YW1(1, "妖怪ウォッチ1"),
    YW2(2, "妖怪ウォッチ2"),
    BUSTERS(3, "妖怪ウォッチバスターズ")
}

object NativeSaveBridge {
    init {
        System.loadLibrary("ykw_save_core")
    }

    private external fun nativeInspect(gameId: Int, data: ByteArray, headData: ByteArray?): String
    private external fun nativeRoundTrip(gameId: Int, data: ByteArray, headData: ByteArray?): ByteArray

    fun inspect(game: GameKind, data: ByteArray, headData: ByteArray?): String =
        nativeInspect(game.nativeId, data, headData)

    fun roundTrip(game: GameKind, data: ByteArray, headData: ByteArray?): ByteArray =
        nativeRoundTrip(game.nativeId, data, headData)
}

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent {
            MaterialTheme {
                Surface(modifier = Modifier.fillMaxSize()) {
                    SaveEditorScreen()
                }
            }
        }
    }
}

@Composable
private fun SaveEditorScreen() {
    val context = androidx.compose.ui.platform.LocalContext.current
    var game by remember { mutableStateOf(GameKind.YW1) }
    var saveName by remember { mutableStateOf<String?>(null) }
    var saveData by remember { mutableStateOf<ByteArray?>(null) }
    var headName by remember { mutableStateOf<String?>(null) }
    var headData by remember { mutableStateOf<ByteArray?>(null) }
    var summary by remember { mutableStateOf("セーブデータを選択してください。") }
    var pendingOutput by remember { mutableStateOf<ByteArray?>(null) }

    val createDocument = rememberLauncherForActivityResult(
        ActivityResultContracts.CreateDocument("application/octet-stream")
    ) { uri ->
        val bytes = pendingOutput
        pendingOutput = null
        if (uri != null && bytes != null) {
            summary = runCatching {
                context.contentResolver.openOutputStream(uri, "wt")?.use { it.write(bytes) }
                    ?: error("出力ストリームを開けませんでした")
                "保存しました: ${displayName(context, uri) ?: uri.lastPathSegment ?: "output"}\n${NativeSaveBridge.inspect(game, bytes, headData)}"
            }.getOrElse { "保存失敗: ${it.message ?: it.javaClass.simpleName}" }
        }
    }

    val openSave = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) {
            runCatching {
                context.contentResolver.openInputStream(uri)?.use { it.readBytes() }
                    ?: error("入力ストリームを開けませんでした")
            }.onSuccess { bytes ->
                saveData = bytes
                saveName = displayName(context, uri) ?: uri.lastPathSegment
            }.onFailure {
                summary = "読込失敗: ${it.message ?: it.javaClass.simpleName}"
            }
        }
    }

    val openHead = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) {
            runCatching {
                context.contentResolver.openInputStream(uri)?.use { it.readBytes() }
                    ?: error("headファイルを開けませんでした")
            }.onSuccess { bytes ->
                headData = bytes
                headName = displayName(context, uri) ?: uri.lastPathSegment
            }.onFailure {
                summary = "head読込失敗: ${it.message ?: it.javaClass.simpleName}"
            }
        }
    }

    LaunchedEffect(game, saveData, headData) {
        val bytes = saveData
        summary = if (bytes == null) {
            "セーブデータを選択してください。"
        } else {
            runCatching { NativeSaveBridge.inspect(game, bytes, headData) }
                .getOrElse { "解析失敗: ${it.message ?: it.javaClass.simpleName}" }
        }
    }

    Column(
        modifier = Modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(20.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        Text("Yo-kai Watch Editors", style = MaterialTheme.typography.headlineMedium)
        Text("Kotlin / Jetpack Compose UI + C++ native save core")
        HorizontalDivider()

        Text("ゲーム", style = MaterialTheme.typography.titleMedium)
        GameKind.entries.forEach { option ->
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                RadioButton(selected = game == option, onClick = { game = option })
                Text(option.label)
            }
        }

        Button(onClick = { openSave.launch(arrayOf("*/*")) }) {
            Text("セーブデータを開く")
        }
        Text(saveName?.let { "選択中: $it (${saveData?.size ?: 0} bytes)" } ?: "未選択")

        if (game == GameKind.BUSTERS) {
            OutlinedButton(onClick = { openHead.launch(arrayOf("*/*")) }) {
                Text("head.yw / head.yw_g を選択")
            }
            Text(headName?.let { "head: $it (${headData?.size ?: 0} bytes)" } ?: "暗号化Bustersセーブではheadファイルが必要です")
        }

        HorizontalDivider()
        Text("C++解析結果", style = MaterialTheme.typography.titleMedium)
        Text(summary)

        Spacer(Modifier.height(4.dp))
        Button(
            enabled = saveData != null,
            onClick = {
                val bytes = saveData ?: return@Button
                runCatching { NativeSaveBridge.roundTrip(game, bytes, headData) }
                    .onSuccess { output ->
                        pendingOutput = output
                        val base = saveName ?: "game.yw"
                        createDocument.launch("roundtrip_$base")
                    }
                    .onFailure {
                        summary = "C++処理失敗: ${it.message ?: it.javaClass.simpleName}"
                    }
            }
        ) {
            Text("C++で検証・再暗号化して保存")
        }

        Text(
            "初期ネイティブコアはセーブの内容を変更せず、復号→構造検証→再暗号化を行います。" +
                "この境界の上に各編集画面をComposeで追加できます。",
            style = MaterialTheme.typography.bodySmall
        )
    }
}

private fun displayName(context: Context, uri: Uri): String? {
    context.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { cursor ->
        val index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
        if (index >= 0 && cursor.moveToFirst()) return cursor.getString(index)
    }
    return null
}
