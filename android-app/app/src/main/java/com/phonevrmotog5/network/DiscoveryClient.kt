// SPDX-License-Identifier: GPL-3.0-only
// PhoneVR-MotoG5 — Android Client
// Copyright (C) 2024 PhoneVR-MotoG5 contributors
//
// network/DiscoveryClient.kt — UDP broadcast discovery.
// Sends a "pvr" + PAIR_HMD packet to the broadcast address and waits
// for a unicast reply from the PC driver.

package com.phonevrmotog5.network

import android.util.Log
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.nio.ByteBuffer
import java.nio.ByteOrder

class DiscoveryClient {

    companion object {
        private const val TAG              = "PhoneVR-Discovery"
        private const val DISCOVERY_PORT   = 33333
        private const val PROTOCOL_VERSION = 1
        private const val MSG_PAIR_HMD: Byte = 0x01
    }

    /**
     * Broadcast a discovery packet and wait for the PC driver to respond.
     * @return The PC's IP address as a String, or null if not found within [timeoutMs].
     */
    fun findPC(timeoutMs: Long = 5000L): String? {
        return try {
            DatagramSocket().use { socket ->
                socket.broadcast   = true
                socket.soTimeout   = timeoutMs.toInt()

                val packet = buildDiscoveryPacket()
                val broadcast = InetAddress.getByName("255.255.255.255")
                socket.send(DatagramPacket(packet, packet.size, broadcast, DISCOVERY_PORT))
                Log.i(TAG, "Discovery packet sent to broadcast:$DISCOVERY_PORT")

                // Wait for ACK from PC driver
                val buf    = ByteArray(64)
                val reply  = DatagramPacket(buf, buf.size)
                socket.receive(reply)
                val pcIp   = reply.address.hostAddress
                Log.i(TAG, "Discovery response from $pcIp")
                pcIp
            }
        } catch (e: Exception) {
            Log.w(TAG, "Discovery failed: ${e.message}")
            null
        }
    }

    private fun buildDiscoveryPacket(): ByteArray {
        // Format: "pvr" (3 bytes) + message_type (1 byte) + version (4 bytes LE)
        val buf = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN)
        buf.put('p'.code.toByte())
        buf.put('v'.code.toByte())
        buf.put('r'.code.toByte())
        buf.put(MSG_PAIR_HMD)
        buf.putInt(PROTOCOL_VERSION)
        return buf.array()
    }
}
