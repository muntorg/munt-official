#### -- Support Library --

#okhttp3
-dontwarn javax.annotation.**
-keepnames class okhttp3.internal.publicsuffix.PublicSuffixDatabase
-dontwarn org.codehaus.mojo.animal_sniffer.*
-dontwarn okhttp3.internal.platform.ConscryptPlatform

-dontwarn org.apache.**
-dontwarn org.apache.commons.logging.**

# support
-dontwarn androidx.support.**
-keep class androidx.support.app.** { *; }
-keep interface androidx.support.app.** { *; }
-keep class androidx.support.** { *; }

# unity backend
-keep class com.gulden.jniunifiedbackend.** { *; }
-keep interface com.gulden.jniunifiedbackend.** { *; }
