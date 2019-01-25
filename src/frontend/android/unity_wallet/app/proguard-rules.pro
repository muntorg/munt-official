# support
-dontwarn androidx.support.**
-keep class androidx.support.app.** { *; }
-keep interface androidx.support.app.** { *; }
-keep class androidx.support.** { *; }

# unity backend
-keep class com.gulden.jniunifiedbackend.** { *; }
-keep interface com.gulden.jniunifiedbackend.** { *; }

#### OkHttp, Retrofit and Moshi
-dontwarn retrofit2.Platform$Java8
-dontwarn okio.**
-dontwarn javax.annotation.**
-keepclasseswithmembers class * { @retrofit2.http.* <methods>; }
-keepclasseswithmembers class * { @com.squareup.moshi.* <methods>; }
-keep @com.squareup.moshi.JsonQualifier interface *
-dontwarn org.jetbrains.annotations.**
-keep class kotlin.Metadata { *; }
-keepclassmembers class kotlin.Metadata { public <methods>; }
-keepclassmembers class * { @com.squareup.moshi.FromJson <methods>; @com.squareup.moshi.ToJson <methods>; }
-keepnames class okhttp3.internal.publicsuffix.PublicSuffixDatabase
-dontwarn org.codehaus.mojo.animal_sniffer.*
-dontwarn okhttp3.internal.platform.ConscryptPlatform

-dontwarn org.apache.**
-dontwarn org.apache.commons.logging.**