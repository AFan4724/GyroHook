package com.example.gyrohook

import android.hardware.Sensor
import de.robv.android.xposed.IXposedHookLoadPackage
import de.robv.android.xposed.IXposedHookZygoteInit
import de.robv.android.xposed.XC_MethodHook
import de.robv.android.xposed.XposedHelpers
import de.robv.android.xposed.XposedBridge
import de.robv.android.xposed.callbacks.XC_LoadPackage
import de.robv.android.xposed.XSharedPreferences
import java.util.concurrent.ConcurrentHashMap

class GyroHook : IXposedHookLoadPackage, IXposedHookZygoteInit {
    companion object {
        private const val TAG = "GyroHook"
        private const val PACKAGE_NAME = "com.example.gyrohook"
        private const val PREF_NAME = "gyro_settings"
        private const val RELOAD_INTERVAL_MS = 500L
    }

    private var prefs: XSharedPreferences? = null

    @Volatile
    private var addRotationX = 0f

    @Volatile
    private var addRotationY = 0f

    @Volatile
    private var addRotationZ = 0f

    @Volatile
    private var lastReloadTime = 0L

    private val gyroHandleCache = ConcurrentHashMap<Int, Boolean>()

    override fun initZygote(startupParam: IXposedHookZygoteInit.StartupParam) {
        prefs = XSharedPreferences(PACKAGE_NAME, PREF_NAME)
        prefs?.makeWorldReadable()
    }

    private fun reloadPreferencesIfNeeded() {
        val now = System.currentTimeMillis()
        if (now - lastReloadTime < RELOAD_INTERVAL_MS) return
        lastReloadTime = now

        try {
            val p = prefs ?: XSharedPreferences(PACKAGE_NAME, PREF_NAME).also { prefs = it }
            p.reload()
            addRotationX = p.getFloat("x", 0f)
            addRotationY = p.getFloat("y", 0f)
            addRotationZ = p.getFloat("z", 0f)
        } catch (e: Exception) {
            XposedBridge.log("$TAG: Error loading preferences: ${e.message}")
        }
    }

    override fun handleLoadPackage(lpparam: XC_LoadPackage.LoadPackageParam) {
        if (lpparam.packageName == PACKAGE_NAME) return

        try {
            reloadPreferencesIfNeeded()

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
                            if (!isGyroscopeSensor(handle, param.thisObject)) return

                            reloadPreferencesIfNeeded()
                            if (addRotationX == 0f && addRotationY == 0f && addRotationZ == 0f) return

                            val values = param.args[1] as FloatArray
                            values[0] += addRotationX
                            values[1] += addRotationY
                            values[2] += addRotationZ
                        } catch (e: Exception) {
                            XposedBridge.log("$TAG: Error in beforeHookedMethod: ${e.message}")
                        }
                    }
                }
            )
        } catch (e: Exception) {
            XposedBridge.log("$TAG: Error in handleLoadPackage: ${e.message}")
        }
    }

    private fun isGyroscopeSensor(handle: Int, eventQueue: Any): Boolean {
        gyroHandleCache[handle]?.let { return it }

        val isGyro = detectGyroscopeSensor(handle, eventQueue)
        gyroHandleCache[handle] = isGyro
        return isGyro
    }

    private fun detectGyroscopeSensor(handle: Int, eventQueue: Any): Boolean {
        try {
            val sensorManager = XposedHelpers.getObjectField(eventQueue, "mManager") ?: return false
            val sensors = XposedHelpers.callMethod(sensorManager, "getSensorList", Sensor.TYPE_ALL) as List<*>
            for (sensor in sensors) {
                if (XposedHelpers.getIntField(sensor, "mHandle") == handle) {
                    return XposedHelpers.getIntField(sensor, "mType") == Sensor.TYPE_GYROSCOPE
                }
            }
        } catch (e: Exception) {
            XposedBridge.log("$TAG: Error checking sensor type: ${e.message}")
        }
        return false
    }
}
