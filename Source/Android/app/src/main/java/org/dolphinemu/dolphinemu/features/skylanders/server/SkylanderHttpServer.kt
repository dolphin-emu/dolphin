// SPDX-License-Identifier: GPL-2.0-or-later

package org.dolphinemu.dolphinemu.features.skylanders.server

import android.content.Context
import android.os.Handler
import android.os.Looper
import org.dolphinemu.dolphinemu.features.skylanders.SkylanderConfig
import org.dolphinemu.dolphinemu.features.skylanders.ui.SkylanderSlot
import org.dolphinemu.dolphinemu.utils.Log
import org.json.JSONArray
import org.json.JSONObject
import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.OutputStream
import java.net.Inet4Address
import java.net.NetworkInterface
import java.net.ServerSocket
import java.net.Socket
import java.net.SocketException
import java.nio.charset.StandardCharsets
import kotlin.concurrent.thread

class SkylanderHttpServer(
    private val context: Context,
    private val port: Int = 9090,
    private val slotListSupplier: () -> List<SkylanderSlot>,
    private val onSlotUpdated: (slotIndex: Int, portalSlot: Int, name: String) -> Unit,
    private val onSlotCleared: (slotIndex: Int) -> Unit
) {
    private var serverSocket: ServerSocket? = null
    @Volatile
    private var isRunning = false
    private val mainHandler = Handler(Looper.getMainLooper())

    fun start() {
        if (isRunning) return

        try {
            serverSocket = ServerSocket(port)
            isRunning = true
            Log.info("Skylander HTTP Server started on port $port")

            thread(name = "SkylanderHttpServerThread", isDaemon = true) {
                while (isRunning) {
                    try {
                        val clientSocket = serverSocket?.accept() ?: break
                        thread(isDaemon = true) {
                            handleClient(clientSocket)
                        }
                    } catch (e: SocketException) {
                        // Server socket closed
                        break
                    } catch (e: Exception) {
                        Log.error("Error accepting client in SkylanderHttpServer: ${e.message}")
                    }
                }
            }
        } catch (e: Exception) {
            Log.error("Failed to start SkylanderHttpServer on port $port: ${e.message}")
        }
    }

    fun stop() {
        isRunning = false
        try {
            serverSocket?.close()
        } catch (_: Exception) {}
        serverSocket = null
        Log.info("Skylander HTTP Server stopped")
    }

    private fun handleClient(socket: Socket) {
        try {
            socket.use { s ->
                val reader = BufferedReader(InputStreamReader(s.getInputStream(), StandardCharsets.UTF_8))
                val output = s.getOutputStream()

                val requestLine = reader.readLine() ?: return
                val parts = requestLine.split(" ")
                if (parts.size < 2) return

                val method = parts[0].uppercase()
                val pathWithQuery = parts[1]
                val path = pathWithQuery.substringBefore("?")

                // Read headers
                var contentLength = 0
                var line: String?
                while (reader.readLine().also { line = it } != null) {
                    if (line!!.isEmpty()) break
                    val headerLower = line!!.lowercase()
                    if (headerLower.startsWith("content-length:")) {
                        contentLength = headerLower.substringAfter("content-length:").trim().toIntOrNull() ?: 0
                    }
                }

                // Read body if present
                var body = ""
                if (contentLength > 0) {
                    val charBuffer = CharArray(contentLength)
                    var totalRead = 0
                    while (totalRead < contentLength) {
                        val read = reader.read(charBuffer, totalRead, contentLength - totalRead)
                        if (read == -1) break
                        totalRead += read
                    }
                    body = String(charBuffer, 0, totalRead)
                }

                if (method == "OPTIONS") {
                    sendResponse(output, 200, "OK", "text/plain", "")
                    return
                }

                when {
                    method == "GET" && (path == "/" || path == "/web" || path == "/index.html") -> {
                        sendResponse(output, 200, "OK", "text/html; charset=utf-8", SkylanderWebUi.HTML)
                    }

                    method == "GET" && path == "/api/status" -> {
                        val responseJson = getStatusJson()
                        sendResponse(output, 200, "OK", "application/json", responseJson.toString())
                    }

                    method == "GET" && path == "/api/skylanders" -> {
                        val catalogJson = getCatalogJson()
                        sendResponse(output, 200, "OK", "application/json", catalogJson.toString())
                    }

                    method == "POST" && path == "/api/load" -> {
                        val result = handleLoadRequest(body)
                        sendResponse(output, if (result.optBoolean("success")) 200 else 400, "OK", "application/json", result.toString())
                    }

                    method == "POST" && path == "/api/remove" -> {
                        val result = handleRemoveRequest(body)
                        sendResponse(output, if (result.optBoolean("success")) 200 else 400, "OK", "application/json", result.toString())
                    }

                    method == "POST" && path == "/api/clear" -> {
                        val result = handleClearAllRequest()
                        sendResponse(output, 200, "OK", "application/json", result.toString())
                    }

                    else -> {
                        val errJson = JSONObject().apply {
                            put("error", "Not Found")
                            put("path", path)
                        }
                        sendResponse(output, 404, "Not Found", "application/json", errJson.toString())
                    }
                }
            }
        } catch (e: Exception) {
            Log.error("Error processing request in SkylanderHttpServer: ${e.message}")
        }
    }

    private fun getStatusJson(): JSONObject {
        val slots = slotListSupplier()
        val json = JSONObject()
        json.put("success", true)
        json.put("running", true)

        val slotsArray = JSONArray()
        slots.forEachIndexed { index, slot ->
            val slotObj = JSONObject()
            slotObj.put("slot", index)
            slotObj.put("portalSlot", slot.portalSlot)
            slotObj.put("name", slot.label)
            slotObj.put("occupied", slot.portalSlot != -1)
            slotsArray.put(slotObj)
        }
        json.put("slots", slotsArray)
        return json
    }

    private fun getCatalogJson(): JSONArray {
        val array = JSONArray()
        for ((pair, name) in SkylanderConfig.LIST_SKYLANDERS) {
            val item = JSONObject()
            item.put("id", pair.id)
            item.put("variant", pair.variant)
            item.put("name", name)
            array.put(item)
        }
        return array
    }

    private fun handleLoadRequest(body: String): JSONObject {
        val response = JSONObject()
        try {
            val json = if (body.isNotBlank()) JSONObject(body) else JSONObject()
            val slotIndex = json.optInt("slot", 0).coerceIn(0, 15)
            val id = json.optInt("id", -1)
            val variant = json.optInt("variant", 0)
            val name = json.optString("name", "")
            val path = json.optString("path", "")

            val slots = slotListSupplier()
            val currentSlot = if (slotIndex < slots.size) slots[slotIndex] else null
            val portalSlotToUse = currentSlot?.portalSlot ?: slotIndex

            var loadResult: android.util.Pair<Int?, String?>? = null

            if (path.isNotBlank()) {
                loadResult = SkylanderConfig.loadSkylander(portalSlotToUse, path)
            } else if (id != -1) {
                loadResult = SkylanderConfig.loadOrAutoCreate(context, id, variant, portalSlotToUse)
            } else if (name.isNotBlank()) {
                loadResult = SkylanderConfig.loadOrAutoCreateByName(context, name, portalSlotToUse)
            }

            if (loadResult != null && loadResult.first != null) {
                val loadedPortalSlot = loadResult.first!!
                val loadedName = loadResult.second ?: "Skylander"

                mainHandler.post {
                    onSlotUpdated(slotIndex, loadedPortalSlot, loadedName)
                }

                response.put("success", true)
                response.put("slot", slotIndex)
                response.put("portalSlot", loadedPortalSlot)
                response.put("name", loadedName)
            } else {
                response.put("success", false)
                response.put("error", "Failed to load figure onto portal slot $slotIndex")
            }
        } catch (e: Exception) {
            response.put("success", false)
            response.put("error", e.message ?: "Unknown error")
        }
        return response
    }

    private fun handleRemoveRequest(body: String): JSONObject {
        val response = JSONObject()
        try {
            val json = if (body.isNotBlank()) JSONObject(body) else JSONObject()
            val slotIndex = json.optInt("slot", 0).coerceIn(0, 15)
            val slots = slotListSupplier()

            if (slotIndex < slots.size) {
                val slot = slots[slotIndex]
                if (slot.portalSlot != -1) {
                    SkylanderConfig.removeSkylander(slot.portalSlot)
                }
                mainHandler.post {
                    onSlotCleared(slotIndex)
                }
            }

            response.put("success", true)
            response.put("slot", slotIndex)
        } catch (e: Exception) {
            response.put("success", false)
            response.put("error", e.message ?: "Unknown error")
        }
        return response
    }

    private fun handleClearAllRequest(): JSONObject {
        val response = JSONObject()
        try {
            val slots = slotListSupplier()
            slots.forEachIndexed { index, slot ->
                if (slot.portalSlot != -1) {
                    SkylanderConfig.removeSkylander(slot.portalSlot)
                }
                mainHandler.post {
                    onSlotCleared(index)
                }
            }
            response.put("success", true)
            response.put("message", "All figures removed")
        } catch (e: Exception) {
            response.put("success", false)
            response.put("error", e.message ?: "Unknown error")
        }
        return response
    }

    private fun sendResponse(
        output: OutputStream,
        statusCode: Int,
        statusText: String,
        contentType: String,
        body: String
    ) {
        val bodyBytes = body.toByteArray(StandardCharsets.UTF_8)
        val header = StringBuilder()
        header.append("HTTP/1.1 $statusCode $statusText\r\n")
        header.append("Content-Type: $contentType\r\n")
        header.append("Content-Length: ${bodyBytes.size}\r\n")
        header.append("Access-Control-Allow-Origin: *\r\n")
        header.append("Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n")
        header.append("Access-Control-Allow-Headers: Content-Type, Authorization\r\n")
        header.append("Connection: close\r\n")
        header.append("\r\n")

        output.write(header.toString().toByteArray(StandardCharsets.UTF_8))
        if (bodyBytes.isNotEmpty()) {
            output.write(bodyBytes)
        }
        output.flush()
    }

    companion object {
        fun getLocalIpAddress(): String? {
            try {
                val interfaces = NetworkInterface.getNetworkInterfaces() ?: return null
                for (intf in interfaces) {
                    if (intf.isLoopback || !intf.isUp) continue
                    for (addr in intf.inetAddresses) {
                        if (!addr.isLoopbackAddress && addr is Inet4Address) {
                            return addr.hostAddress
                        }
                    }
                }
            } catch (_: Exception) {}
            return null
        }
    }
}
