// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — Android Client
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// MainActivity.kt — Connection setup UI.
// Provides "Find PC" (UDP auto-discovery) and manual IP entry,
// then launches VRActivity when a connection is established.

package com.phonevrmotog5

import android.content.Intent
import android.hardware.Sensor
import android.hardware.SensorManager
import android.os.Bundle
import android.view.View
import android.widget.Button
import android.widget.EditText
import android.widget.ProgressBar
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import com.phonevrmotog5.network.DiscoveryClient
import kotlinx.coroutines.*

class MainActivity : AppCompatActivity() {

    private lateinit var btnFind:      Button
    private lateinit var btnConnect:   Button
    private lateinit var etIp:         EditText
    private lateinit var tvStatus:     TextView
    private lateinit var tvGyroWarn:   TextView
    private lateinit var progressBar:  ProgressBar

    private val scope = CoroutineScope(Dispatchers.Main + SupervisorJob())

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        btnFind     = findViewById(R.id.btn_find_pc)
        btnConnect  = findViewById(R.id.btn_connect)
        etIp        = findViewById(R.id.et_ip_address)
        tvStatus    = findViewById(R.id.tv_status)
        tvGyroWarn  = findViewById(R.id.tv_gyro_warning)
        progressBar = findViewById(R.id.progress_bar)

        checkGyroscope()

        btnFind.setOnClickListener { startDiscovery() }
        btnConnect.setOnClickListener {
            val ip = etIp.text.toString().trim()
            if (ip.isNotEmpty()) {
                launchVR(ip)
            } else {
                Toast.makeText(this, R.string.enter_ip_hint, Toast.LENGTH_SHORT).show()
            }
        }
    }

    private fun checkGyroscope() {
        val sensorManager = getSystemService(SENSOR_SERVICE) as SensorManager
        val hasGyro = sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE) != null
        if (!hasGyro) {
            tvGyroWarn.visibility = View.VISIBLE
            tvGyroWarn.setText(R.string.no_gyro_warning)
        }
    }

    private fun startDiscovery() {
        tvStatus.setText(R.string.discovering)
        progressBar.visibility = View.VISIBLE
        btnFind.isEnabled      = false

        scope.launch {
            val ip = withContext(Dispatchers.IO) {
                DiscoveryClient().findPC(timeoutMs = 5000L)
            }
            progressBar.visibility = View.GONE
            btnFind.isEnabled      = true
            if (ip != null) {
                tvStatus.text = getString(R.string.found_pc, ip)
                launchVR(ip)
            } else {
                tvStatus.setText(R.string.discovery_failed)
                Toast.makeText(this@MainActivity, R.string.discovery_failed, Toast.LENGTH_LONG).show()
            }
        }
    }

    private fun launchVR(pcIp: String) {
        val intent = Intent(this, VRActivity::class.java)
        intent.putExtra(VRActivity.EXTRA_PC_IP, pcIp)
        startActivity(intent)
    }

    override fun onDestroy() {
        super.onDestroy()
        scope.cancel()
    }
}
