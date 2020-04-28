package com.cubist.dualnetworktest

import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.util.TypedValue
import kotlinx.android.synthetic.main.activity_main.*

class MainActivity : AppCompatActivity()
{
    override fun onCreate( savedInstanceState: Bundle? )
    {
        super.onCreate( savedInstanceState )

        setContentView( R.layout.activity_main )

        nativeInit();

        info_text.text = "There are four main cases to test for dual network mode.\n\n" +
                         "  - A socket bound explicitly to wlan0.\n\n" +
                         "        nc -u wlanAddr wlanPort\n\n" +
                         "  - A socket bound explicitly to eth0.\n\n" +
                         "        nc -u ethAddr ethPort\n\n" +
                         "  - A socket bound to 0.0.0.0, connecting through wlan0.\n\n" +
                         "        nc -u wlanAddr anyPort\n\n" +
                         "  - A socket bound to 0.0.0.0, connecting through etho0.\n\n" +
                         "        nc -u ethAddr anyPort\n\n"
                info_text.setTextSize( TypedValue.COMPLEX_UNIT_SP, 32f )

        sample_text.text = launchSockets()
        sample_text.setTextSize( TypedValue.COMPLEX_UNIT_SP, 42f )
    }

    fun updateText( text: String? )
    {
        sample_text.text = text
    }

    external fun nativeInit()
    external fun launchSockets(): String

    companion object
    {
        // Used to load the 'native-lib' library on application startup.
        init
        {
            System.loadLibrary("native-lib")
        }
    }
}
