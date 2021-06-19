
set JNI_GENERATOR_PY="base/android/jni_generator/jni_generator.py"
set JNI_GENERATOR_HELPER_H=base/android/jni_generator/jni_generator_helper.h

rem set src_dir=base/android/java/src/org/chromium/base
set src_dir=net/android/java/src/org/chromium/net
set dst_dir=jni
echo section: %src_dir%

rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/ApkAssets.java --output_file %dst_dir%/ApkAssets_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/ApplicationStatus.java --output_file %dst_dir%/ApplicationStatus_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/BuildInfo.java --output_file %dst_dir%/BuildInfo_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/BundleUtils.java --output_file %dst_dir%/BundleUtils_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/Callback.java --output_file %dst_dir%/Callback_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/CommandLine.java --output_file %dst_dir%/CommandLine_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/ContentUriUtils.java --output_file %dst_dir%/ContentUriUtils_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/CpuFeatures.java --output_file %dst_dir%/CpuFeatures_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/EarlyTraceEvent.java --output_file %dst_dir%/EarlyTraceEvent_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/EventLog.java --output_file %dst_dir%/EventLog_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/FeatureList.java --output_file %dst_dir%/FeatureList_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/FieldTrialList.java --output_file %dst_dir%/FieldTrialList_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/ImportantFileWriterAndroid.java --output_file %dst_dir%/ImportantFileWriterAndroid_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/IntStringCallback.java --output_file %dst_dir%/IntStringCallback_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/JNIUtils.java --output_file %dst_dir%/JNIUtils_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/JavaExceptionReporter.java --output_file %dst_dir%/JavaExceptionReporter_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/JavaHandlerThread.java --output_file %dst_dir%/JavaHandlerThread_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/LocaleUtils.java --output_file %dst_dir%/LocaleUtils_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/MemoryPressureListener.java --output_file %dst_dir%/MemoryPressureListener_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/PathService.java --output_file %dst_dir%/PathService_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/PathUtils.java --output_file %dst_dir%/PathUtils_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/PowerMonitor.java --output_file %dst_dir%/PowerMonitor_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/RadioUtils.java --output_file %dst_dir%/RadioUtils_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/SysUtils.java --output_file %dst_dir%/SysUtils_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/ThreadUtils.java --output_file %dst_dir%/ThreadUtils_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/TimeUtils.java --output_file %dst_dir%/TimeUtils_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/TimezoneUtils.java --output_file %dst_dir%/TimezoneUtils_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/TraceEvent.java --output_file %dst_dir%/TraceEvent_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/UnguessableToken.java --output_file %dst_dir%/UnguessableToken_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/library_loader/LibraryLoader.java --output_file %dst_dir%/LibraryLoader_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/library_loader/LibraryPrefetcher.java --output_file %dst_dir%/LibraryPrefetcher_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/memory/JavaHeapDumpGenerator.java --output_file %dst_dir%/JavaHeapDumpGenerator_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/metrics/NativeUmaRecorder.java --output_file %dst_dir%/NativeUmaRecorder_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/metrics/RecordHistogram.java --output_file %dst_dir%/RecordHistogram_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/metrics/RecordUserAction.java --output_file %dst_dir%/RecordUserAction_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/metrics/StatisticsRecorderAndroid.java --output_file %dst_dir%/StatisticsRecorderAndroid_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/process_launcher/ChildProcessService.java --output_file %dst_dir%/ChildProcessService_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/task/PostTask.java --output_file %dst_dir%/PostTask_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/task/TaskRunnerImpl.java --output_file %dst_dir%/TaskRunnerImpl_jni.h



echo section: %src_dir%
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/AndroidCellularSignalStrength.java --output_file %dst_dir%/AndroidCellularSignalStrength_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/AndroidCertVerifyResult.java --output_file %dst_dir%/AndroidCertVerifyResult_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/AndroidKeyStore.java --output_file %dst_dir%/AndroidKeyStore_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/AndroidNetworkLibrary.java --output_file %dst_dir%/AndroidNetworkLibrary_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/AndroidTrafficStats.java --output_file %dst_dir%/AndroidTrafficStats_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/DnsStatus.java --output_file %dst_dir%/DnsStatus_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/GURLUtils.java --output_file %dst_dir%/GURLUtils_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/HttpNegotiateAuthenticator.java --output_file %dst_dir%/HttpNegotiateAuthenticator_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/HttpUtil.java --output_file %dst_dir%/HttpUtil_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/NetStringUtil.java --output_file %dst_dir%/NetStringUtil_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/NetworkActivationRequest.java --output_file %dst_dir%/NetworkActivationRequest_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/NetworkChangeNotifier.java --output_file %dst_dir%/NetworkChangeNotifier_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/ProxyChangeListener.java --output_file %dst_dir%/ProxyChangeListener_jni.h
rem %JNI_GENERATOR_PY% --includes %JNI_GENERATOR_HELPER_H% --input_file %src_dir%/X509Util.java --output_file %dst_dir%/X509Util_jni.h