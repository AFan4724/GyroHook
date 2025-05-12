package com.example.gyrohook

import android.hardware.Sensor
import android.hardware.SensorEvent
import android.util.Log
import de.robv.android.xposed.IXposedHookLoadPackage
import de.robv.android.xposed.IXposedHookZygoteInit
import de.robv.android.xposed.XC_MethodHook
import de.robv.android.xposed.XposedHelpers
import de.robv.android.xposed.XposedBridge
import de.robv.android.xposed.callbacks.XC_LoadPackage
import de.robv.android.xposed.XSharedPreferences
import java.io.File

class GyroHook : IXposedHookLoadPackage, IXposedHookZygoteInit {
    companion object {
        private const val TAG = "GyroHook"
        private const val PACKAGE_NAME = "com.example.gyrohook"
        private const val PREF_NAME = "gyro_settings"
        
        private fun loadPreferences(): Triple<Float, Float, Float> {
            try {
                val prefs = XSharedPreferences(PACKAGE_NAME, PREF_NAME)
                if (!prefs.file.canRead()) {
                    XposedBridge.log("$TAG: Cannot read preferences file")
                    return Triple(0f, 0f, 0f)
                }
                
                val x = prefs.getFloat("x", 0f)
                val y = prefs.getFloat("y", 0f)
                val z = prefs.getFloat("z", 0f)
                
                XposedBridge.log("$TAG: Successfully loaded values - X: $x, Y: $y, Z: $z")
                return Triple(x, y, z)
            } catch (e: Exception) {
                XposedBridge.log("$TAG: Error loading preferences: ${e.message}")
                return Triple(0f, 0f, 0f)
            }
        }
    }

    private lateinit var modulePath: String
    private var prefs: XSharedPreferences? = null

    private var addRotationX = 0f
    private var addRotationY = 0f
    private var addRotationZ = 0f

    override fun initZygote(startupParam: IXposedHookZygoteInit.StartupParam) {
        modulePath = startupParam.modulePath
        prefs = XSharedPreferences("com.example.gyrohook", PREF_NAME)
        prefs?.makeWorldReadable()
        XposedBridge.log("$TAG: Initialized in Zygote with module path: $modulePath")
    }

    private fun loadPreferences() {
        try {
            XposedBridge.log("$TAG: Starting to load preferences")
            
            prefs?.reload()
            
            val prefsFile = File("/data/user/0/com.example.gyrohook/shared_prefs/${PREF_NAME}.xml")
            XposedBridge.log("$TAG: Preferences file exists: ${prefsFile.exists()}")
            XposedBridge.log("$TAG: Preferences file readable: ${prefsFile.canRead()}")
            if (prefsFile.exists()) {
                XposedBridge.log("$TAG: Preferences file path: ${prefsFile.absolutePath}")
            }
            
            val (x, y, z) = Companion.loadPreferences()
            
            addRotationX = x
            addRotationY = y
            addRotationZ = z
            
            XposedBridge.log("$TAG: Loaded values - X: $addRotationX, Y: $addRotationY, Z: $addRotationZ")
            
            if (addRotationX == 0f && addRotationY == 0f && addRotationZ == 0f) {
                XposedBridge.log("$TAG: Warning - All values are 0, might indicate loading failure")
                prefs = XSharedPreferences("com.example.gyrohook", PREF_NAME)
                prefs?.makeWorldReadable()
            }
        } catch (e: Exception) {
            XposedBridge.log("$TAG: Error loading preferences: ${e.message}")
            e.printStackTrace()
        }
    }

    override fun handleLoadPackage(lpparam: XC_LoadPackage.LoadPackageParam) {
        XposedBridge.log("$TAG: Module initialized for package: ${lpparam.packageName}")
        
        try {
            loadPreferences()
            
            XposedBridge.log("$TAG: Starting gyroscope hook with additional values - X: $addRotationX, Y: $addRotationY, Z: $addRotationZ")

            XposedHelpers.findAndHookMethod(
                "android.hardware.SystemSensorManager\$SensorEventQueue",
                lpparam.classLoader,
                "dispatchSensorEvent",
                Int::class.java,
                FloatArray::class.java,
                Int::class.java,
                Long::class.java,
                object : XC_MethodHook() {
                    override fun beforeHookedMethod(param: MethodHookParam) {
                        try {
                            val handle = param.args[0] as Int
                            val values = param.args[1] as FloatArray

                            if (isGyroscopeSensor(handle, param.thisObject)) {
                                loadPreferences()
                                
                                values[0] += addRotationX
                                values[1] += addRotationY
                                values[2] += addRotationZ
                                
                                XposedBridge.log("$TAG: Modified gyroscope values - Original+Added: X: ${values[0]}, Y: ${values[1]}, Z: ${values[2]}")
                            }
                        } catch (e: Exception) {
                            XposedBridge.log("$TAG: Error in beforeHookedMethod: ${e.message}")
                            e.printStackTrace()
                        }
                    }
                }
            )
            XposedBridge.log("$TAG: Successfully hooked sensor method")
        } catch (e: Exception) {
            XposedBridge.log("$TAG: Error in handleLoadPackage: ${e.message}")
            e.printStackTrace()
        }
    }

    private fun isGyroscopeSensor(handle: Int, eventQueue: Any): Boolean {
        try {
            val sensorManager = XposedHelpers.getObjectField(eventQueue, "mManager")
            if (sensorManager != null) {
                val sensors = XposedHelpers.callMethod(sensorManager, "getSensorList", Sensor.TYPE_ALL) as List<*>
                for (sensor in sensors) {
                    val sensorHandle = XposedHelpers.getIntField(sensor, "mHandle")
                    if (sensorHandle == handle) {
                        val type = XposedHelpers.getIntField(sensor, "mType")
                        val isGyro = type == Sensor.TYPE_GYROSCOPE
                        if (isGyro) {
                            XposedBridge.log("$TAG: Found gyroscope sensor with handle: $handle")
                        }
                        return isGyro
                    }
                }
            }
        } catch (e: Exception) {
            XposedBridge.log("$TAG: Error checking sensor type: ${e.message}")
        }
        return false
    }
}