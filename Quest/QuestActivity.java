package nl.yf.smallgiveawaybike1.vr;

import android.app.NativeActivity;
import android.content.res.AssetFileDescriptor;
import android.content.res.AssetManager;
import android.media.AudioAttributes;
import android.media.MediaPlayer;
import android.media.SoundPool;
import android.os.Bundle;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;

/* Meta Quest-versie: de native code (main_quest.c, libmain.so) doet alles; Java levert alleen geluid
 * (SoundPool voor effecten en lussen, MediaPlayer voor muziek en stemmen) en internet (httpGet). */
public class QuestActivity extends NativeActivity {
    static {
        System.loadLibrary("openxr_loader");
    }

    private static QuestActivity inst;
    private static SoundPool pool;
    private static int[] sfxIds = new int[0], loopIds = new int[0], loopStream = new int[0];
    private static MediaPlayer music, voice;
    private static boolean musicOn;

    @Override
    protected void onCreate(Bundle b) {
        inst = this;
        super.onCreate(b);
    }

    private static AssetFileDescriptor fd(String name) throws Exception {
        AssetManager am = inst.getAssets();
        return am.openFd(name);
    }

    public static void audioInit(String[] sfx, String[] loops) {
        try {
            AudioAttributes aa = new AudioAttributes.Builder().setUsage(AudioAttributes.USAGE_GAME)
                    .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION).build();
            pool = new SoundPool.Builder().setMaxStreams(20).setAudioAttributes(aa).build();
            sfxIds = new int[sfx.length];
            for (int i = 0; i < sfx.length; i++) {
                try { sfxIds[i] = pool.load(fd("sfx_" + sfx[i] + ".wav"), 1); } catch (Exception e) { sfxIds[i] = 0; }
            }
            loopIds = new int[loops.length];
            loopStream = new int[loops.length];
            for (int i = 0; i < loops.length; i++) {
                try { loopIds[i] = pool.load(fd("sfx_" + loops[i] + ".wav"), 1); } catch (Exception e) { loopIds[i] = 0; }
            }
            AssetFileDescriptor m = fd("music.mp3");
            music = new MediaPlayer();
            music.setDataSource(m.getFileDescriptor(), m.getStartOffset(), m.getLength());
            music.setLooping(true);
            music.prepare();
            music.setVolume(0.55f, 0.55f);
        } catch (Throwable t) {
            android.util.Log.w("SGB", "audio: " + t);
        }
    }

    public static void sfx(int id, float vol) {
        if (pool == null || id < 0 || id >= sfxIds.length || sfxIds[id] == 0) return;
        pool.play(sfxIds[id], vol, vol, 1, 0, 1.0f);
    }

    public static void loop(int id, float vol, float rate) {
        if (pool == null || id < 0 || id >= loopIds.length || loopIds[id] == 0) return;
        float r = Math.max(0.5f, Math.min(2.0f, rate));
        if (vol <= 0.01f) {
            if (loopStream[id] != 0) { pool.stop(loopStream[id]); loopStream[id] = 0; }
            return;
        }
        if (loopStream[id] == 0) loopStream[id] = pool.play(loopIds[id], vol, vol, 0, -1, r);
        else { pool.setVolume(loopStream[id], vol, vol); pool.setRate(loopStream[id], r); }
    }

    private static boolean voicePlaying() {
        try { return voice != null && voice.isPlaying(); } catch (Throwable t) { return false; }
    }

    public static void music(boolean on) {
        if (music == null) return;
        try {
            float v = voicePlaying() ? 0.22f : 0.55f;   /* zachter tijdens een stem */
            music.setVolume(v, v);
            if (on && !musicOn) { music.start(); musicOn = true; }
            else if (!on && musicOn) { music.pause(); musicOn = false; }
        } catch (Throwable t) { }
    }

    public static void voice(String key, float vol) {
        try {
            if (voice != null) { voice.release(); voice = null; }
            if (key == null || vol <= 0.01f) return;
            AssetFileDescriptor f = fd("vo_" + key + ".mp3");
            voice = new MediaPlayer();
            voice.setDataSource(f.getFileDescriptor(), f.getStartOffset(), f.getLength());
            voice.prepare();
            voice.setVolume(Math.min(1f, vol), Math.min(1f, vol));
            voice.start();
        } catch (Throwable t) {
            android.util.Log.w("SGB", "stem " + key + ": " + t);
        }
    }

    /* Haalt een URL op; null bij een fout of een andere status dan 200. */
    public static byte[] httpGet(String u) {
        HttpURLConnection c = null;
        try {
            c = (HttpURLConnection) new URL(u).openConnection();
            c.setConnectTimeout(15000);
            c.setReadTimeout(90000);
            c.setInstanceFollowRedirects(true);
            c.setRequestProperty("User-Agent", "SmallGiveawayBike1/1.0 (Quest)");
            if (c.getResponseCode() != 200) return null;
            InputStream in = new BufferedInputStream(c.getInputStream());
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] b = new byte[16384];
            int n;
            while ((n = in.read(b)) > 0) out.write(b, 0, n);
            in.close();
            return out.toByteArray();
        } catch (Throwable t) {
            return null;
        } finally {
            if (c != null) c.disconnect();
        }
    }
}
