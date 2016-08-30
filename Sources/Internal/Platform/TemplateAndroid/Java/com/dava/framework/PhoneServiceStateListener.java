package com.dava.framework;

import com.dava.engine.DavaActivity;

import android.content.Context;
import android.telephony.PhoneStateListener;
import android.util.Log;
import android.telephony.TelephonyManager;
import android.telephony.ServiceState;

public class PhoneServiceStateListener extends PhoneStateListener {
    final static String TAG = "PhoneServiceStateListener";

    private String carrierName = GetCarrierName();
    
    private native void OnCarrierNameChanged(); 

    @Override
    public void onDataConnectionStateChanged(int state, int networkType) {
        String newCarrierName = GetCarrierName();
        if (!newCarrierName.equals(carrierName))
        {
            carrierName = newCarrierName;
            OnCarrierNameChanged();
        }
        super.onDataConnectionStateChanged(state, networkType);
    }

    public String GetCarrierName()
    {
        TelephonyManager manager;
        if (JNIActivity.GetActivity() != null)
        {
            manager = (TelephonyManager)JNIActivity.GetActivity().getSystemService(Context.TELEPHONY_SERVICE);
        }
        else
        {
            manager = (TelephonyManager)DavaActivity.instance().getSystemService(Context.TELEPHONY_SERVICE);
        }
        return manager.getSimOperatorName();
    }
}
