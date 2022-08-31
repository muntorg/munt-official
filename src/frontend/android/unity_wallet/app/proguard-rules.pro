# Support
# Below is probably overkill, but keep it like this for now; we can slowly tune it to be less aggressive later.
# Otherwise there were issues with ActionMode and other things
-dontwarn com.google.android.material.**
-keep class com.google.android.material.** { *; }
-dontwarn androidx.**
-keep class androidx.** { *; }
-keep interface androidx.** { *; }
-dontwarn android.support.v4.**
-keep class android.support.v4.** { *; }
-dontwarn android.support.v7.**
-keep class android.support.v7.** { *; }

# unity backend
-keep class unity_wallet.jniunifiedbackend.** { *; }
-keep interface unity_wallet.jniunifiedbackend.** { *; }

#### OkHttp, Retrofit and Moshi
-dontwarn okio.**
-dontwarn javax.annotation.**
-keepclasseswithmembers class * { @retrofit2.http.* <methods>; }
-keepnames class okhttp3.internal.publicsuffix.PublicSuffixDatabase
-dontwarn org.codehaus.mojo.animal_sniffer.*
-dontwarn okhttp3.internal.platform.ConscryptPlatform

-dontwarn org.apache.**
-dontwarn org.apache.commons.logging.**
