// Copyright (c) 2018-2019 The Gulden developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com), Willem de Jonge (willem@isnapp.nl)
// Distributed under the GULDEN software license, see the accompanying
// file COPYING

package unity_wallet

import android.content.Intent
import android.content.SharedPreferences
import android.net.Uri
import android.os.Bundle
import android.view.View
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.core.widget.TextViewCompat
import androidx.fragment.app.Fragment
import androidx.preference.PreferenceManager
import com.android.volley.*
import com.android.volley.toolbox.StringRequest
import com.android.volley.toolbox.Volley
import com.google.android.material.bottomnavigation.BottomNavigationView
import com.google.android.material.snackbar.Snackbar
import jniunifiedbackend.ILibraryController
import jniunifiedbackend.IWalletController
import unity_wallet.main_activity_fragments.*
import unity_wallet.ui.getDisplayDimensions
import unity_wallet.ui.monitor.NetworkMonitorActivity
import unity_wallet.util.AppBaseActivity
import unity_wallet.util.getAndroidVersion
import unity_wallet.util.getDeviceName
import kotlinx.android.synthetic.main.activity_main.*
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import org.json.JSONObject
import kotlin.concurrent.thread


class WalletActivity : UnityCore.Observer, AppBaseActivity(),
        SharedPreferences.OnSharedPreferenceChangeListener
{
    override fun syncProgressChanged(percent: Float) {
        setSyncProgress(percent)
    }

    override fun onCreate(savedInstanceState: Bundle?)
    {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.activity_main)

        navigation.setOnNavigationItemSelectedListener(mOnNavigationItemSelectedListener)

        syncProgress.max = 1000000
        syncProgressTextual.text = ""


        val preferences = PreferenceManager.getDefaultSharedPreferences(this)
        preferences.registerOnSharedPreferenceChangeListener(this)

        topLayoutBarSettingsBackButton.setOnClickListener { onBackPressed() }
    }

    override fun onDestroy() {
        super.onDestroy()

        try
        {
            coroutineContext[Job]!!.cancel()
            val preferences = PreferenceManager.getDefaultSharedPreferences(this)
            preferences.unregisterOnSharedPreferenceChangeListener(this)
        }
        catch (e : Exception)
        {

        }
    }

    override fun onStart() {
        super.onStart()
        UnityCore.instance.addObserver(this@WalletActivity, fun (callback:() -> Unit) { runOnUiThread { callback() }})

        if (supportFragmentManager.fragments.isEmpty()) {
            addFragment(SendFragment(), R.id.mainLayout)
        }
    }

    override fun onWalletReady() {
        setSyncProgress(UnityCore.instance.progressPercent)
        setWalletBalance(UnityCore.instance.balanceAmount)
    }

    override fun onStop() {
        super.onStop()

        UnityCore.instance.removeObserver(this)
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences?, key: String?) {
        when (key) {
            "preference_local_currency" -> {
                setWalletBalance(UnityCore.instance.balanceAmount)
            }
        }
    }

    fun onRequestAdvancedSettings(view: View? = null)
    {
        val intent = Intent(this, NetworkMonitorActivity::class.java)
        startActivity(intent)
    }

    fun onRequestSupport(view: View? = null)
    {
        try
        {
            val i = Intent(Intent.ACTION_SENDTO)
            i.type = "message/rfc822"
            i.data = Uri.parse("mailto:")
            i.putExtra(Intent.EXTRA_EMAIL, arrayOf("support@gulden.com"))
            i.putExtra(Intent.EXTRA_SUBJECT, "Support request")
            i.putExtra(Intent.EXTRA_TEXT, getDeviceName() + " / " + getAndroidVersion() + " / " +  getString(R.string.about_text_app_name) + System.getProperty("line.separator") )
            startActivity(Intent.createChooser(i, "Send mail..."))
        }
        catch (ex: android.content.ActivityNotFoundException)
        {
            Toast.makeText(this, "No email app installed.", Toast.LENGTH_SHORT).show()
        }

    }

    fun onRequestLicense(view: View? = null)
    {
        startActivity(Intent(this, LicenseActivity::class.java))
    }

    private fun gotoPage(page: Fragment) {
        gotoFragment(page, R.id.mainLayout)
    }

    fun pushWalletSettingsPage()
    {
        pushFragment(WalletSettingsFragment(), R.id.mainLayout)
    }

    fun pushCurrencyPage()
    {
        pushFragment(LocalCurrencyFragment(), R.id.mainLayout)
    }

    private val mOnNavigationItemSelectedListener = BottomNavigationView.OnNavigationItemSelectedListener { item ->
        val page =
         when (item.itemId) {
            R.id.navigation_send -> SendFragment()
            R.id.navigation_receive -> ReceiveFragment()
            R.id.navigation_transactions -> MutationFragment()
            R.id.navigation_settings -> SettingsFragment()
             else -> return@OnNavigationItemSelectedListener false
        }
        gotoPage(page)
        true
    }

    fun performLink(linkURI: String)
    {
        Authentication.instance.authenticate(this, null, getString(R.string.link_wallet_auth_desc)) { password ->
            // ReplaceWalletLinkedFromURI can be long running, so run it in a thread that isn't the UI thread.
            thread(start = true)
            {
                if (!ILibraryController.ReplaceWalletLinkedFromURI(linkURI, password.joinToString("")))
                {
                    runOnUiThread {
                        AlertDialog.Builder(this)
                                .setTitle(getString(R.string.no_guldensync_warning_title))
                                .setMessage(getString(R.string.no_guldensync_warning))
                                .setPositiveButton(getString(R.string.button_ok)) {
                                    dialogInterface, i -> dialogInterface.dismiss()
                                }.setCancelable(true).create().show()
                    }
                }
                else
                {
                    runOnUiThread {
                        Snackbar.make(View(this@WalletActivity), getString(R.string.rescan_started), Snackbar.LENGTH_SHORT).show()
                        gotoPage(ReceiveFragment())
                    }
                }
            }
        }
    }


    private fun setSyncProgress(percent: Float)
    {
        val textualProgress = if (percent <= 0.0) getString(R.string.label_sync_progress_connecting) else getString(R.string.label_sync_progress_syncing).format(percent)
        syncProgress.progress = (syncProgress.max * (percent/100)).toInt()
        syncProgressTextual.text = textualProgress
        if (percent < 100.0)
        {
            syncProgressTextual.visibility = View.VISIBLE
            syncProgress.visibility = View.VISIBLE
        }
        else
        {
            syncProgressTextual.visibility = View.INVISIBLE
            syncProgress.visibility = View.INVISIBLE
        }
    }

    private fun setWalletBalance(balance : Long)
    {
        val coins = balance.toDouble() / Config.COIN
        walletBalance.text = String.format("G %.2f", coins)
        walletBalance.visibility = View.VISIBLE

        this.launch( Dispatchers.Main) {
            try {
                val rate = fetchCurrencyRate(localCurrency.code)
                walletBalanceLocal.text = String.format(" (${localCurrency.short} %.${localCurrency.precision}f)",
                        coins * rate)
                walletBalanceLocal.visibility = View.VISIBLE

                // set auto size text if native and local amounts will not fity on a single line
                // this will only kick in in very very rare circumstances
                // like huge balance (> G 10M and very limited device with largest font setting)
                // (before the sync text was moved out of balance display it was a lot easier to trigger)
                val outSize = getDisplayDimensions(this@WalletActivity)
                balanceSection.measure(outSize.x, outSize.y)
                val preferredWidth = balanceSection.measuredWidth
                TextViewCompat.setAutoSizeTextTypeWithDefaults(
                        walletBalanceLocal,
                        if (preferredWidth > outSize.x)  TextView.AUTO_SIZE_TEXT_TYPE_UNIFORM else TextView.AUTO_SIZE_TEXT_TYPE_NONE)
                balanceSection.invalidate()
            }
            catch (e: Throwable) {
                walletBalanceLocal.text = ""
                walletBalanceLocal.visibility = View.GONE
            }
        }
    }

    override fun walletBalanceChanged(balance: Long) {
        setWalletBalance(balance)
    }

    fun showSettingsTitle(title : String)
    {
        topLayoutBarSettingsHeader.visibility = View.VISIBLE
        topLayoutBar.visibility = View.GONE
        topLayoutBarSettingsTitle.text = title
    }

    fun hideSettingsTitle()
    {
        topLayoutBarSettingsHeader.visibility = View.GONE
        topLayoutBar.visibility = View.VISIBLE
    }

    @Suppress("UNUSED_PARAMETER")
    fun gotoBuyActivity(view : View? = null)
    {
        // Send a post request to blockhut with our wallet/address info; and then launch the site if we get a positive response.
        val MyRequestQueue = Volley.newRequestQueue(this)
        val failURL = "https://gulden.com/buy"
        val request = object : StringRequest(Request.Method.POST,"https://blockhut.com/buysession.php",
            Response.Listener { response ->
                try
                {
                    var jsonResponse = JSONObject(response);
                    if (jsonResponse.getInt("status_code") == 200)
                    {
                        var sessionID = jsonResponse.getString("sessionid")
                        val intent = Intent(Intent.ACTION_VIEW, Uri.parse("https://blockhut.com/buy.php?sessionid=%s".format(sessionID)))
                        startActivity(intent)
                    }
                    else
                    {
                        // Redirect user to the default fallback site
                        //fixme: Do something with the status message here
                        //var statusMessage = jsonResponse.getString("status_message")
                        val intent = Intent(failURL)
                        if (intent.resolveActivity(packageManager) != null)
                        {
                            startActivity(intent)
                        }
                    }
                }
                catch (e:Exception)
                {
                    // Redirect user to the default fallback site
                    //fixme: Do something with the error message here
                    val intent = Intent(failURL)
                    startActivity(intent)
                }
            },
            Response.ErrorListener
            {
               // If we are sure its a local connectivity issue, alert the user, otherwise send them to the default fallback site
               if (it is NetworkError || it is AuthFailureError || it is NoConnectionError)
               {
                   Toast.makeText(this, getString(R.string.error_check_internet_connection), Toast.LENGTH_SHORT).show()
               }
               else
               {
                   val intent = Intent(failURL)
                   startActivity(intent)
               }
            }
        )
        // Force values to be send at x-www-form-urlencoded
        {
            override fun getParams(): MutableMap<String, String> {
                val params = HashMap<String,String>()
                params["address"] = ILibraryController.GetReceiveAddress().toString();
                params["uuid"] = IWalletController.GetUUID();
                params["currency"] = "gulden";
                params["wallettype"] = "android";
                return params;
            }
            override fun getHeaders(): MutableMap<String, String> {
                val params = HashMap<String, String>();
                params.put("Content-Type","application/x-www-form-urlencoded");
                return params;
            }
        };

        // Volley request policy, only one time request to avoid duplicate transaction
        request.retryPolicy = DefaultRetryPolicy(
                DefaultRetryPolicy.DEFAULT_TIMEOUT_MS,
                1, // DefaultRetryPolicy.DEFAULT_MAX_RETRIES = 2
                1f // DefaultRetryPolicy.DEFAULT_BACKOFF_MULT
        )

        // Add the volley post request to the request queue
        MyRequestQueue.add(request)
    }
}
