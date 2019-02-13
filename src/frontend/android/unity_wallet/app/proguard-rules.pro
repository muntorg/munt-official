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

# guldenj
-keep,includedescriptorclasses class org.guldenj.wallet.Protos$** { *; }
-keepclassmembers class org.guldenj.wallet.Protos { com.google.protobuf.Descriptors$FileDescriptor descriptor; }
-keep,includedescriptorclasses class org.bitcoin.protocols.payments.Protos$** { *; }
-keepclassmembers class org.bitcoin.protocols.payments.Protos { com.google.protobuf.Descriptors$FileDescriptor descriptor; }
-dontwarn org.guldenj.store.WindowsMMapHack
-dontwarn org.guldenj.store.LevelDBBlockStore
-dontnote org.guldenj.crypto.DRMWorkaround
-dontnote org.guldenj.crypto.TrustStoreLoader$DefaultTrustStoreLoader
-dontnote com.subgraph.orchid.crypto.PRNGFixes
-dontwarn okio.DeflaterSink
-dontwarn okio.Okio
-dontnote com.squareup.okhttp.internal.Platform
-dontwarn org.guldenj.store.LevelDBFullPrunedBlockStore**
-dontwarn org.guldenj.net.**
-dontwarn org.guldenj.core.PeerGroup

# java-wns-resolver
-dontwarn com.netki.WalletNameResolver
-dontwarn com.netki.dns.DNSBootstrapService
-dontnote org.xbill.DNS.ResolverConfig
-dontwarn org.xbill.DNS.spi.DNSJavaNameServiceDescriptor
-dontnote org.xbill.DNS.spi.DNSJavaNameServiceDescriptor
-dontwarn org.apache.log4j.**

# Guava
-dontwarn sun.misc.Unsafe
-dontwarn com.google.common.collect.MinMaxPriorityQueue
-dontwarn com.google.common.util.concurrent.FuturesGetChecked**
-dontwarn javax.lang.model.element.Modifier
-dontwarn afu.org.checkerframework.**
-dontwarn org.checkerframework.**
-dontwarn java.lang.invoke**

# slf4j
-dontwarn org.slf4j.MDC
-dontwarn org.slf4j.MarkerFactory

# logback-android
-dontwarn javax.mail.**
-dontnote ch.qos.logback.core.rolling.helper.FileStoreUtil

# Bitcoin Wallet
-dontnote com.gulden.wallet.util.Io
