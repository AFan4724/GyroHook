package com.example.gyrohook

import android.content.Context
import android.os.Bundle
import android.view.Menu
import android.view.MenuItem
import android.widget.Button
import android.widget.EditText
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.Toolbar
import androidx.core.content.ContextCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsControllerCompat
import android.util.Log
import java.net.ServerSocket
import java.net.Socket
import kotlinx.coroutines.*
import com.google.android.material.switchmaterial.SwitchMaterial
import java.net.BindException
import java.net.SocketException
import java.io.File
import org.json.JSONObject
import java.io.FileWriter
import com.example.gyrohook.databinding.ActivityMainBinding
import java.io.BufferedReader
import java.io.InputStreamReader
import java.util.Locale

class MainActivity : AppCompatActivity() {
    private lateinit var binding: ActivityMainBinding
    private lateinit var etRotationX: EditText
    private lateinit var etRotationY: EditText
    private lateinit var etRotationZ: EditText
    private lateinit var etPort: EditText
    private lateinit var btnApply: Button
    private lateinit var switchSocket: SwitchMaterial

    private var serverSocket: ServerSocket? = null
    private var serverJob: Job? = null
    private val coroutineScope = CoroutineScope(Dispatchers.IO + Job())
    private var isServerRunning = false
    private val PREF_NAME = "gyro_settings"
    private val PREF_APP = "app_settings"

    companion object {
        private const val TAG = "GyroHook"
        private const val PREF_ROTATION_X = "rotation_x"
        private const val PREF_ROTATION_Y = "rotation_y"
        private const val PREF_ROTATION_Z = "rotation_z"
        private const val PREF_PORT = "socket_port"
        private const val DEFAULT_PORT = 16384
    }

    override fun attachBaseContext(newBase: Context) {
        val prefs = newBase.getSharedPreferences("app_settings", MODE_PRIVATE)
        val lang = prefs.getString("app_language", "en") ?: "en"
        val locale = if (lang == "zh") Locale.CHINESE else Locale.ENGLISH
        val config = newBase.resources.configuration
        config.setLocale(locale)
        val context = newBase.createConfigurationContext(config)
        super.attachBaseContext(context)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        WindowCompat.setDecorFitsSystemWindows(window, true)
        window.statusBarColor = ContextCompat.getColor(this, R.color.primary)
        window.navigationBarColor = ContextCompat.getColor(this, R.color.background_color)
        WindowInsetsControllerCompat(window, window.decorView).apply {
            isAppearanceLightStatusBars = false
            isAppearanceLightNavigationBars = true
        }

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        val toolbar: Toolbar = binding.toolbar
        setSupportActionBar(toolbar)

        try {
            val prefs = getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE)
            binding.etRotationX.setText(prefs.getFloat("x", 0f).toString())
            binding.etRotationY.setText(prefs.getFloat("y", 0f).toString())
            binding.etRotationZ.setText(prefs.getFloat("z", 0f).toString())
        } catch (e: Exception) {
            Log.e("GyroHook", "Failed to load preferences", e)
            Toast.makeText(this, getString(R.string.failed_load_settings), Toast.LENGTH_SHORT).show()
        }

        initializeViews()
        loadSettings()
        setupApplyButton()
        setupSocketSwitch()
        ensureFilePermissions()
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        menuInflater.inflate(R.menu.menu_main, menu)
        return true
    }

    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return when (item.itemId) {
            R.id.action_language -> {
                showLanguageDialog()
                true
            }
            else -> super.onOptionsItemSelected(item)
        }
    }

    private fun showLanguageDialog() {
        val prefs = getSharedPreferences(PREF_APP, MODE_PRIVATE)
        val current = prefs.getString("app_language", "en") ?: "en"
        val options = arrayOf(getString(R.string.language_en), getString(R.string.language_zh))
        val currentIndex = if (current == "zh") 1 else 0

        AlertDialog.Builder(this)
            .setTitle(getString(R.string.language_dialog_title))
            .setSingleChoiceItems(options, currentIndex) { dialog, which ->
                val lang = if (which == 1) "zh" else "en"
                prefs.edit().putString("app_language", lang).apply()
                dialog.dismiss()
                recreate()
            }
            .show()
    }

    private fun ensureFilePermissions() {
        try {
            val settingsFile = File(dataDir, PREF_NAME)
            if (!settingsFile.exists()) {
                val defaultSettings = JSONObject().apply {
                    put(PREF_ROTATION_X, 0f)
                    put(PREF_ROTATION_Y, 0f)
                    put(PREF_ROTATION_Z, 0f)
                    put(PREF_PORT, DEFAULT_PORT)
                }
                FileWriter(settingsFile).use { it.write(defaultSettings.toString()) }
            }
            settingsFile.setReadable(true, false)
            Log.d(TAG, "Settings file path: ${settingsFile.absolutePath}")
        } catch (e: Exception) {
            Log.e(TAG, "Error ensuring file permissions: ${e.message}")
        }
    }

    private fun initializeViews() {
        try {
            etRotationX = binding.etRotationX
            etRotationY = binding.etRotationY
            etRotationZ = binding.etRotationZ
            etPort = binding.etPort
            btnApply = binding.btnApply
            switchSocket = binding.switchSocket
        } catch (e: Exception) {
            Log.e(TAG, "Error initializing views: ${e.message}")
            throw e
        }
    }

    private fun loadSettings() {
        try {
            val prefs = getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE)
            etRotationX.setText(prefs.getFloat("x", 0f).toString())
            etRotationY.setText(prefs.getFloat("y", 0f).toString())
            etRotationZ.setText(prefs.getFloat("z", 0f).toString())
            etPort.setText(prefs.getInt("socket_port", DEFAULT_PORT).toString())
            Log.d(TAG, "Settings loaded successfully")
        } catch (e: Exception) {
            Log.e(TAG, "Error loading settings: ${e.message}")
            etRotationX.setText("0")
            etRotationY.setText("0")
            etRotationZ.setText("0")
            etPort.setText(DEFAULT_PORT.toString())
        }
    }

    private fun saveSettings(x: Float, y: Float, z: Float, port: Int = DEFAULT_PORT) {
        try {
            val prefs = getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE)
            prefs.edit().apply {
                putFloat("x", x)
                putFloat("y", y)
                putFloat("z", z)
                putInt("socket_port", port)
                apply()
            }

            val dataDir = File(applicationInfo.dataDir)
            val sharedPrefsDir = File(dataDir, "shared_prefs")
            val prefsFile = File(sharedPrefsDir, "${PREF_NAME}.xml")

            if (prefsFile.exists()) {
                prefsFile.setReadable(true, false)
                sharedPrefsDir.setExecutable(true, false)
                sharedPrefsDir.setReadable(true, false)
                Log.d(TAG, "Set permissions for: ${prefsFile.absolutePath}")
            }

            Log.d(TAG, "Settings saved - X: $x, Y: $y, Z: $z, Port: $port")
        } catch (e: Exception) {
            Log.e(TAG, "Error saving settings: ${e.message}")
            Toast.makeText(this, getString(R.string.save_failed) + e.message, Toast.LENGTH_SHORT).show()
        }
    }

    private fun setupApplyButton() {
        btnApply.setOnClickListener {
            try {
                val x = etRotationX.text.toString().toFloatOrNull() ?: 0f
                val y = etRotationY.text.toString().toFloatOrNull() ?: 0f
                val z = etRotationZ.text.toString().toFloatOrNull() ?: 0f
                val port = etPort.text.toString().toIntOrNull() ?: DEFAULT_PORT
                saveSettings(x, y, z, port)
                Toast.makeText(this, getString(R.string.settings_saved), Toast.LENGTH_SHORT).show()
            } catch (e: Exception) {
                Log.e(TAG, "Error saving settings: ${e.message}")
                Toast.makeText(this, getString(R.string.save_failed) + e.message, Toast.LENGTH_SHORT).show()
            }
        }
    }

    private fun setupSocketSwitch() {
        switchSocket.setOnCheckedChangeListener { _, isChecked ->
            if (isChecked && !isServerRunning) {
                startSocketServer()
            } else if (!isChecked && isServerRunning) {
                stopSocketServer()
            }
        }
    }

    private fun startSocketServer() {
        try {
            val port = etPort.text.toString().toIntOrNull() ?: DEFAULT_PORT
            if (port < 1024 || port > 65535) {
                throw IllegalArgumentException("端口号必须在1024-65535之间")
            }

            serverJob = coroutineScope.launch(Dispatchers.IO + CoroutineExceptionHandler { _, e ->
                Log.e(TAG, "Server coroutine error: ${e.message}")
                handleServerError(e)
            }) {
                try {
                    serverSocket = ServerSocket(port)
                    isServerRunning = true
                    Log.d(TAG, "Socket server started on port $port")

                    withContext(Dispatchers.Main) {
                        Toast.makeText(this@MainActivity, getString(R.string.socket_started) + port, Toast.LENGTH_SHORT).show()
                    }

                    while (isActive && isServerRunning) {
                        try {
                            val socket = serverSocket?.accept()
                            if (socket != null) {
                                handleClientConnection(socket)
                            }
                        } catch (e: SocketException) {
                            if (isServerRunning) {
                                Log.e(TAG, "Socket accept error: ${e.message}")
                            }
                            break
                        }
                    }
                } catch (e: BindException) {
                    Log.e(TAG, "Port binding error: ${e.message}")
                    handleServerError(e)
                } catch (e: Exception) {
                    Log.e(TAG, "Server error: ${e.message}")
                    handleServerError(e)
                } finally {
                    closeServerSocket()
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error starting socket server: ${e.message}")
            handleServerError(e)
        }
    }

    private fun handleClientConnection(socket: Socket) {
        coroutineScope.launch(Dispatchers.IO + CoroutineExceptionHandler { _, e ->
            Log.e(TAG, "Client connection error: ${e.message}")
            socket.close()
        }) {
            try {
                val reader = BufferedReader(InputStreamReader(socket.getInputStream()))

                while (isActive && isServerRunning) {
                    try {
                        val line = reader.readLine()

                        if (line != null) {
                            val parts = line.split(',')
                            if (parts.size == 3) {
                                val x = parts[0].toFloatOrNull()
                                val y = parts[1].toFloatOrNull()
                                val z = parts[2].toFloatOrNull()

                                Log.d(TAG, "Received data: x=$x, y=$y, z=$z")

                                if (x != null && y != null && z != null) {
                                    withContext(Dispatchers.Main) {
                                        etRotationX.setText(x.toString())
                                        etRotationY.setText(y.toString())
                                        etRotationZ.setText(z.toString())
                                        saveSettings(x, y, z)
                                    }
                                } else {
                                    Log.e(TAG, "Error parsing floats from string: $line")
                                }
                            } else {
                                Log.w(TAG, "Received malformed line: $line")
                            }
                        } else {
                            Log.d(TAG, "Client disconnected or end of stream.")
                            break
                        }
                    } catch (e: SocketException) {
                        if (isServerRunning) {
                            Log.e(TAG, "Client read error: ${e.message}")
                        }
                        break
                    } catch (e: NumberFormatException) {
                        Log.e(TAG, "Cannot parse float from received string", e)
                    }
                }
            } catch (e: Exception) {
                Log.e(TAG, "Client connection error: ${e.message}")
            } finally {
                try {
                    socket.close()
                } catch (e: Exception) {
                    Log.e(TAG, "Error closing client socket: ${e.message}")
                }
            }
        }
    }

    private fun handleServerError(e: Throwable) {
        val errorMessage = when (e) {
            is BindException -> "端口${etPort.text}已被占用，请更换端口"
            is IllegalArgumentException -> e.message ?: "端口号无效"
            else -> "服务器错误：${e.message}"
        }

        coroutineScope.launch(Dispatchers.Main) {
            Toast.makeText(this@MainActivity, errorMessage, Toast.LENGTH_LONG).show()
            switchSocket.isChecked = false
            isServerRunning = false
        }
    }

    private fun closeServerSocket() {
        try {
            serverSocket?.close()
            serverSocket = null
            isServerRunning = false
        } catch (e: Exception) {
            Log.e(TAG, "Error closing server socket: ${e.message}")
        }
    }

    private fun stopSocketServer() {
        try {
            isServerRunning = false
            serverJob?.cancel()
            closeServerSocket()
            Log.d(TAG, "Socket server stopped")
            Toast.makeText(this, getString(R.string.socket_stopped), Toast.LENGTH_SHORT).show()
        } catch (e: Exception) {
            Log.e(TAG, "Error stopping socket server: ${e.message}")
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        stopSocketServer()
        coroutineScope.cancel()
    }
}
