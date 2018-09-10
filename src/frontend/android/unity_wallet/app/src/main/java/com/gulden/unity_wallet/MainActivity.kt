package com.gulden.unity_wallet

import android.os.Bundle
import android.support.design.widget.BottomNavigationView
import android.support.v7.app.AppCompatActivity
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import kotlinx.android.synthetic.main.activity_main.*
import kotlin.concurrent.thread

class MainActivity : AppCompatActivity() {

    private val mOnNavigationItemSelectedListener = BottomNavigationView.OnNavigationItemSelectedListener { item ->
        when (item.itemId) {
            R.id.navigation_send -> {
                return@OnNavigationItemSelectedListener true
            }
            R.id.navigation_receive -> {
                return@OnNavigationItemSelectedListener true
            }
            R.id.navigation_transactions -> {
                return@OnNavigationItemSelectedListener true
            }
            R.id.navigation_settings -> {
                return@OnNavigationItemSelectedListener true
            }
        }
        false
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        thread(start = true) {
            System.loadLibrary("gulden_unity_jni")
            GuldenUnifiedBackend.InitUnityLib(applicationContext.getApplicationInfo().dataDir)
        }
        navigation.setOnNavigationItemSelectedListener(mOnNavigationItemSelectedListener)
    }
}
