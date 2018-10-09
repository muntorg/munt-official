package com.gulden.unity_wallet.ui.buy

import android.content.Intent
import android.graphics.Bitmap
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.support.v4.app.Fragment
import android.support.v7.app.AppCompatActivity
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.webkit.WebResourceError
import android.webkit.WebResourceRequest
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.ProgressBar
import android.widget.TextView
import com.gulden.unity_wallet.R

//import com.gulden.wallet.util.FontUtils.enableAssetFontsForWebView


class BuyFragment : Fragment()
{
    private var buyAddress: String? = null
    private var webView: WebView? = null
    private var errorView: TextView? = null
    private var progressBar: ProgressBar? = null

    private val buyURL = "https://gulden.com/purchase"

    override fun onCreate(savedInstanceState: Bundle?)
    {
        super.onCreate(savedInstanceState)
        if (arguments != null)
        {
            buyAddress = arguments?.getString(ARG_BUY_ADDRESS)
        }
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View?
    {
        return inflater.inflate(R.layout.fragment_buy, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?)
    {
        super.onViewCreated(view, savedInstanceState)

        progressBar = view.findViewById(R.id.buyProgressIndicator)
        webView = view.findViewById(R.id.buyWebView)
        errorView = view.findViewById(R.id.buyViewErrorText)

        progressBar?.isIndeterminate = true

        progressBar?.visibility = View.VISIBLE
        webView?.visibility = View.GONE
        errorView?.visibility = View.GONE

        webView?.setBackgroundColor(0x00000000)
        webView?.settings?.javaScriptEnabled = true
        webView?.loadUrl(buyURL)


        // Handle page load
        LoadBuyPage()

        //Sort action bar out
        run {
            val actionBar = (activity as AppCompatActivity).supportActionBar
            actionBar?.setDisplayShowTitleEnabled(true)
            actionBar?.setDisplayShowCustomEnabled(true)
            actionBar?.setDisplayShowHomeEnabled(true)
            actionBar?.setDisplayHomeAsUpEnabled(true)
        }
    }

    fun LoadBuyPage()
    {
        webView?.webViewClient = object : WebViewClient()
        {

            override fun shouldOverrideUrlLoading(view: WebView, urlNewString: String): Boolean
            {
                if (!urlNewString.toLowerCase().startsWith("https://gulden.com"))
                {
                    val webpage = Uri.parse(urlNewString)
                    val intent = Intent(Intent.ACTION_VIEW, webpage)
                    if (activity != null && activity?.packageManager != null && intent.resolveActivity(activity?.packageManager) != null)
                    {
                        startActivity(intent)
                        activity?.finish()
                    }
                }

                webView?.loadUrl(urlNewString)
                return true
            }

            override fun onPageStarted(view: WebView, url: String, favicon: Bitmap?)
            {
                progressBar?.visibility = View.VISIBLE
                webView?.visibility = View.GONE
                errorView?.visibility = View.GONE
            }

            override fun onPageFinished(view: WebView, url: String)
            {

                // Insert fontAwesome and other fonts.
                //enableAssetFontsForWebView(webView, activity)

                // Let the webpage know it is embedded in our app and should display itself accordingly.
                view.loadUrl(String.format("javascript:if(NocksBuyFormFillDetails){NocksBuyFormFillDetails('%s', '%s');$(\".extra-row\").hide();}", buyAddress, ""), null)

                //Slight delay to prevent any flicker
                val handler = Handler()
                handler.postDelayed({
                    progressBar?.visibility = View.GONE
                    errorView?.visibility = View.GONE
                    webView?.visibility = View.VISIBLE
                }, 250)
            }

            override fun onReceivedError(view: WebView, request: WebResourceRequest, error: WebResourceError)
            {
                progressBar?.visibility = View.GONE
                webView?.visibility = View.GONE
                errorView?.visibility = View.VISIBLE
                //fixme: clickable
            }
        }
    }

    internal fun clearURL()
    {
        webView?.loadUrl("")
    }

    internal fun acceptPurchase()
    {
        webView?.loadUrl("javascript:$('.submit').click();")
    }

    companion object
    {
        private val ARG_BUY_ADDRESS = "buy_address"

        fun newInstance(buyAddress: String): BuyFragment
        {
            val fragment = BuyFragment()
            val args = Bundle()
            args.putString(ARG_BUY_ADDRESS, buyAddress)
            fragment.arguments = args
            return fragment
        }
    }
}
