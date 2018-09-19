package com.gulden.unity_wallet

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.support.design.widget.BottomNavigationView
import android.support.v4.app.Fragment
import android.support.v4.app.FragmentManager
import android.support.v4.app.FragmentTransaction
import android.support.v7.app.AppCompatActivity
import android.view.View
import com.google.android.gms.common.api.CommonStatusCodes
import com.google.android.gms.vision.barcode.Barcode
import com.gulden.barcodereader.BarcodeCaptureActivity
import com.gulden.jniunifiedbackend.GuldenUnifiedBackend
import com.gulden.jniunifiedbackend.GuldenUnifiedFrontendImpl
import com.gulden.jniunifiedbackend.UriRecord
import com.gulden.unity_wallet.MainActivityFragments.ReceiveFragment
import com.gulden.unity_wallet.MainActivityFragments.SendFragment
import com.gulden.unity_wallet.MainActivityFragments.SendFragment.OnFragmentInteractionListener
import com.gulden.unity_wallet.MainActivityFragments.SettingsFragment
import com.gulden.unity_wallet.MainActivityFragments.TransactionFragment
import kotlinx.android.synthetic.main.activity_main.*
import java.util.*
import kotlin.concurrent.thread



inline fun FragmentManager.inTransaction(func: FragmentTransaction.() -> FragmentTransaction) {
    beginTransaction().func().commit()
}

fun AppCompatActivity.addFragment(fragment: Fragment, frameId: Int){
    supportFragmentManager.inTransaction { add(frameId, fragment) }
}

fun AppCompatActivity.replaceFragment(fragment: Any, frameId: Int) {
    supportFragmentManager.inTransaction{replace(frameId, fragment as Fragment)}
}

class MainActivity : AppCompatActivity(), OnFragmentInteractionListener, ReceiveFragment.OnFragmentInteractionListener, TransactionFragment.OnFragmentInteractionListener, SettingsFragment.OnFragmentInteractionListener  {

    override fun onFragmentInteraction(uri: Uri) {
        TODO("not implemented")
    }

    private val sendFragment : SendFragment = SendFragment();
    private val receiveFragment : ReceiveFragment = ReceiveFragment();
    private val transactionFragment : TransactionFragment = TransactionFragment();
    private val settingsFragment : SettingsFragment = SettingsFragment();

    private val mOnNavigationItemSelectedListener = BottomNavigationView.OnNavigationItemSelectedListener { item ->
        when (item.itemId) {
            R.id.navigation_send -> {
                replaceFragment(sendFragment, R.id.mainLayout)
                return@OnNavigationItemSelectedListener true
            }
            R.id.navigation_receive -> {
                replaceFragment(receiveFragment, R.id.mainLayout)
                return@OnNavigationItemSelectedListener true
            }
            R.id.navigation_transactions -> {
                replaceFragment(transactionFragment, R.id.mainLayout)
                return@OnNavigationItemSelectedListener true
            }
            R.id.navigation_settings -> {
                replaceFragment(settingsFragment, R.id.mainLayout)
                return@OnNavigationItemSelectedListener true
            }
        }
        false
    }

    private val coreSignals: GuldenUnifiedFrontendImpl?=GuldenUnifiedFrontendImpl()


    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        coreSignals?.activity = this

        thread(start = true) {
            System.loadLibrary("gulden_unity_jni")
            GuldenUnifiedBackend.InitUnityLib(applicationContext.getApplicationInfo().dataDir, coreSignals)
        }
        navigation.setOnNavigationItemSelectedListener(mOnNavigationItemSelectedListener)

        syncProgress.setMax(1000000);
        syncProgress.setProgress(0);

        addFragment(sendFragment, R.id.mainLayout)
    }

    public fun updateSyncProgressPercent(percent : Float)
    {
        syncProgress.setProgress((1000000 * (percent/100)).toInt());
    }

    public fun updateBalance(balance : Long)
    {
        walletBalance.setText((balance.toFloat() / 100000000).toString())
        walletBalanceLogo.visibility = View.VISIBLE;
        walletBalance.visibility = View.VISIBLE;
        walletLogo.visibility = View.GONE;
    }

    override fun onStop() {
        super.onStop()
    }

    override fun onDestroy() {
        super.onDestroy()
    }

    fun handleQRScanButtonClick(view : View) {
        val intent = Intent(applicationContext, BarcodeCaptureActivity::class.java)
        startActivityForResult(intent, BARCODE_READER_REQUEST_CODE)
    }

    fun Uri.getParameters(): HashMap<String, String> {
        val items : HashMap<String, String> = HashMap<String, String>();
        if (isOpaque())
            return items;

        val query = encodedQuery ?: return items;

        var start = 0
        do {
            val nextAmpersand = query.indexOf('&', start)
            val end = if (nextAmpersand != -1) nextAmpersand else query.length

            var separator = query.indexOf('=', start)
            if (separator > end || separator == -1) {
                separator = end
            }

            if (separator == end) {
                items[Uri.decode(query.substring(start, separator))] = "";
            } else {
                items[Uri.decode(query.substring(start, separator))] = Uri.decode(query.substring(separator + 1, end));
            }

            // Move start to end of name.
            if (nextAmpersand != -1) {
                start = nextAmpersand + 1
            } else {
                break
            }
        } while (true)
        return items
    }
    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        if (requestCode == BARCODE_READER_REQUEST_CODE) {
            if (resultCode == CommonStatusCodes.SUCCESS)
            {
                if (data != null) {
                    val barcode = data.getParcelableExtra<Barcode>(BarcodeCaptureActivity.BarcodeObject)

                    val parsedQRCodeURI = Uri.parse(barcode.displayValue);
                    var address : String = "";
                    address += parsedQRCodeURI?.authority;
                    address += parsedQRCodeURI?.path;
                    val parsedQRCodeURIRecord = UriRecord(parsedQRCodeURI.scheme, address , parsedQRCodeURI.getParameters())
                    if (GuldenUnifiedBackend.IsValidRecipient(parsedQRCodeURIRecord)) {
                        val intent = Intent(applicationContext, SendCoinsActivity::class.java)
                        intent.putExtra(SendCoinsActivity.EXTRA_RECIPIENT_ADDRESS, address);
                        if (parsedQRCodeURIRecord.items.containsKey("amount"))
                        {
                            intent.putExtra(SendCoinsActivity.EXTRA_RECIPIENT_AMOUNT, parsedQRCodeURIRecord.items["amount"]);
                        }
                        startActivityForResult(intent, SEND_COINS_RETURN_CODE)
                    }
                }
            }
        } else {
            super.onActivityResult(requestCode, resultCode, data)
        }
    }

    companion object {
        private val BARCODE_READER_REQUEST_CODE = 1
        private val SEND_COINS_RETURN_CODE = 2
    }
}
