package com.sky.SkyEmu;


import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.text.InputType;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.Window;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;

import android.app.NativeActivity;
import android.widget.EditText;
import android.widget.FrameLayout;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Locale;
import java.util.Vector;

public class EnhancedNativeActivity extends NativeActivity {
    final static int APP_STORAGE_ACCESS_REQUEST_CODE = 501; // Any value
    final static int STORAGE_PERMISSION_CODE = 501; // Any value
    final static int FILE_PICKER_REQUEST_CODE = 123;
    final static String TAG="SkyEmu"; // Any value
    public Rect visibleRect;
    public EditText invisibleEditText;
    private Vector<Integer> keyboardEvents;
    static {
        System.loadLibrary("SkyEmu");
    }
    public void requestPermissions() {
    }
    public float getDPIScale(){
        DisplayMetrics metrics = getResources().getDisplayMetrics();
        getWindowManager().getDefaultDisplay().getRealMetrics(metrics);
        return metrics.xdpi/120.0f;
    }
    public String getLanguage() {
        return Locale.getDefault().toString();
    }
    /*Handle permission request results*/
    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == STORAGE_PERMISSION_CODE){
            if (grantResults.length > 0){
                //check each permission if granted or not
                boolean write = grantResults[0] == PackageManager.PERMISSION_GRANTED;
                boolean read = grantResults[1] == PackageManager.PERMISSION_GRANTED;

                if (write && read){
                    //External Storage permissions granted
                    Log.d(TAG, "onRequestPermissionsResult: External Storage permissions granted");
                }
                else{
                    //External Storage permission denied
                    Log.d(TAG, "onRequestPermissionsResult: External Storage permission denied");
                }
            }
        }
    }
    public float getVisibleBottom(){
        return visibleRect.bottom;
    }
    public float getVisibleTop(){
        return visibleRect.top;
    }
    public int getEvent(){
        if(keyboardEvents.isEmpty())return -1;
        int val = keyboardEvents.get(0);
        keyboardEvents.remove(0);
        return val;
    }
    public void showKeyboard(){
        Window win =this.getWindow();
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                win.clearFlags(WindowManager.LayoutParams.FLAG_ALT_FOCUSABLE_IM);
                invisibleEditText.requestFocus();
                InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
                imm.showSoftInput(invisibleEditText, InputMethodManager.SHOW_IMPLICIT);
            }
        });
    }

    public void hideKeyboard()
    {
        Window win =this.getWindow();
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                win.setFlags(WindowManager.LayoutParams.FLAG_ALT_FOCUSABLE_IM,
                        WindowManager.LayoutParams.FLAG_ALT_FOCUSABLE_IM);
                InputMethodManager imm = ( InputMethodManager )getSystemService( Context.INPUT_METHOD_SERVICE );
                imm.hideSoftInputFromWindow( win.getDecorView().getWindowToken(), InputMethodManager.HIDE_IMPLICIT_ONLY);
                //win.setFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE,WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE);
            }
        });
    }
    public void pollKeyboard(){
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                keyboardEvents.add(-2);
                int pre = 0;
                boolean inserted = false;
                if(invisibleEditText.getSelectionEnd()!= invisibleEditText.getText().length()-8){
                    int distance =  invisibleEditText.getText().length()-invisibleEditText.getSelectionEnd()-8;
                    for(int c=0; c<distance;++c ){
                        //Left Arrow
                        keyboardEvents.add(1| 0x40000000);
                    }
                    distance =  invisibleEditText.getSelectionEnd()-(invisibleEditText.getText().length()-8);
                    for(int c=0; c<distance;++c ){
                        //Right Arrow
                        keyboardEvents.add(2| 0x40000000);
                    }
                }
                for (int c : invisibleEditText.getText().toString().chars().toArray()) {
                    if (pre < 8) {
                        if (c == '\1') {
                            pre++;
                            continue;
                        } else {
                            while (pre < 8) {
                                inserted=true;
                                // Backspace
                                keyboardEvents.add(11 | 0x40000000);
                                pre++;
                            }
                        }
                    }
                    if(pre>=invisibleEditText.getText().length()-8){
                        break;
                    }
                    pre++;
                    inserted=true;
                    //Enter
                    if(c=='\n'){keyboardEvents.add(13 |0x40000000);
                    }else keyboardEvents.add(c);
                }
                while (pre < 8) {
                    inserted=true;
                    keyboardEvents.add(11 | 0x40000000);
                    pre++;
                }
                if(inserted) {
                    invisibleEditText.setText("\1\1\1\1\1\1\1\1\2\2\2\2\2\2\2\2");
                    invisibleEditText.setSelection(invisibleEditText.getText().length()-8);
                }
                if(invisibleEditText.getSelectionEnd()!= invisibleEditText.getText().length()-8)
                    invisibleEditText.setSelection(invisibleEditText.getText().length()-8);
            }
        });
    }
    private File copyFileToExternalDirectory(Uri sourceFilePath, String destinationDirectoryPath, String filename) {
        File sourceFile = new File(sourceFilePath.getPath());
        if(sourceFile!=null)Log.i("FilePicker","Source File Exists\n");
        File destinationDirectory = new File(destinationDirectoryPath);
        if(destinationDirectory!=null)Log.i("FilePicker","Destination File Exists\n");

        if (!destinationDirectory.exists()) {
            destinationDirectory.mkdirs();
        }

        File copiedFile = new File(destinationDirectory, filename);
        if(copiedFile!=null)Log.i("FilePicker","Copied File Exists\n");

        try (InputStream in = getContentResolver().openInputStream(sourceFilePath);
             OutputStream out = new FileOutputStream(copiedFile)) {
            byte[] buffer = new byte[1024];
            int length;
            while ((length = in.read(buffer)) > 0) {
                out.write(buffer, 0, length);
            }
            Log.i("FilePicker","Done copying\n");
            return copiedFile;
        } catch (IOException e) {
            e.printStackTrace();
            return null;
        }
    }
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Window mRootWindow = getWindow();
        FrameLayout.LayoutParams mRparams = new FrameLayout.LayoutParams(FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT);
        invisibleEditText = new EditText(this);
        invisibleEditText.setLayoutParams(mRparams);
        invisibleEditText.setRawInputType(InputType.TYPE_CLASS_TEXT);
        invisibleEditText.setImeOptions(EditorInfo.IME_FLAG_NO_EXTRACT_UI);
        keyboardEvents = new Vector<Integer>(5);
        mRootWindow.setFlags(WindowManager.LayoutParams.FLAG_ALT_FOCUSABLE_IM,
                WindowManager.LayoutParams.FLAG_ALT_FOCUSABLE_IM);
        View mRootView = mRootWindow.getDecorView().findViewById(android.R.id.content);
        ((FrameLayout)mRootView).addView(invisibleEditText);

        EnhancedNativeActivity activity = this;
        mRootView.getViewTreeObserver().addOnGlobalLayoutListener(
            new ViewTreeObserver.OnGlobalLayoutListener() {
                public void onGlobalLayout(){
                    Rect r = new Rect();
                    View view = mRootWindow.getDecorView();
                    view.getWindowVisibleDisplayFrame(r);
                    activity.visibleRect = r;
                }
            });

        int currentApiVersion = android.os.Build.VERSION.SDK_INT;

        final int flags = View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;

        // This work only for android 4.4+
        if(currentApiVersion >= Build.VERSION_CODES.KITKAT){
            getWindow().getDecorView().setSystemUiVisibility(flags);

            // Code below is to handle presses of Volume up or Volume down.
            // Without this, after pressing volume buttons, the navigation bar will
            // show up and won't hide
            final View decorView = getWindow().getDecorView();
            decorView
                    .setOnSystemUiVisibilityChangeListener(new View.OnSystemUiVisibilityChangeListener()
                    {
                        @Override
                        public void onSystemUiVisibilityChange(int visibility)
                        {
                            if((visibility & View.SYSTEM_UI_FLAG_FULLSCREEN) == 0)
                            {
                                decorView.setSystemUiVisibility(flags);
                            }
                        }
                    });
        }

    }
    public void openFile(){
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.setType("*/*");
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION
                | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
        startActivityForResult(intent, FILE_PICKER_REQUEST_CODE);
    }
    public String getFileName(Uri uri) {
        String result = null;
        if (uri.getScheme().equals("content")) {
            Cursor cursor = getContentResolver().query(uri, null, null, null, null);
            try {
                if (cursor != null && cursor.moveToFirst()) {
                    result = cursor.getString(cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME));
                }
            } finally {
                cursor.close();
            }
        }
        if (result == null) {
            result = uri.getPath();
            int cut = result.lastIndexOf('/');
            if (cut != -1) {
                result = result.substring(cut + 1);
            }
        }
        return result;
    }
    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        // If the selection didn't work
        if (resultCode != RESULT_OK) {
            // Exit without doing anything else
            return;
        } else {
            if (requestCode == FILE_PICKER_REQUEST_CODE && data != null) {
                Uri selectedFileUri = data.getData();
                String filename = getFileName(selectedFileUri);
                File file = new File(selectedFileUri.getPath());//create path from uri
                Log.i("FilePicker", "Selected file path: " + filename);

                if (selectedFileUri != null) {
                    // Get the original file's path using its URI
                    // Copy the file to the external directory
                    String externalDirectoryPath = getExternalFilesDir(null).getAbsolutePath();
                    File copiedFile = copyFileToExternalDirectory(selectedFileUri, externalDirectoryPath,filename);

                    if (copiedFile != null) {
                        String copiedFilePath = copiedFile.getAbsolutePath();
                        se_android_load_file(copiedFilePath);
                        Log.i("FilePicker", "Copied file path: " + copiedFilePath);
                    }
                }
            }
        }
    }
    public native void se_android_load_file(String filePath);
}
