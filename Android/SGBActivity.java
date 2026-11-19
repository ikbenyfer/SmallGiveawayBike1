package nl.yf.smallgiveawaybike1;

import org.libsdl.app.SDLActivity;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;

/* SDL-activity van Small Giveaway Bike 1. httpGet wordt vanuit C (main_win.c) aangeroepen voor de echte NL-kaart. */
public class SGBActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] {"SDL2", "SDL2_mixer", "main"};
    }

    /* Haalt een URL op; null bij een fout of een andere status dan 200. */
    public static byte[] httpGet(String u) {
        HttpURLConnection c = null;
        try {
            c = (HttpURLConnection) new URL(u).openConnection();
            c.setConnectTimeout(15000);
            c.setReadTimeout(90000);
            c.setInstanceFollowRedirects(true);
            c.setRequestProperty("User-Agent", "SmallGiveawayBike1/1.0 (Android)");
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
