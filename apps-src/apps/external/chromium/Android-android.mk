SUB_PATH := external/chromium

LOCAL_SRC_FILES += \
	$(SUB_PATH)/base/android/android_hardware_buffer_compat.cc \
	$(SUB_PATH)/base/android/android_image_reader_compat.cc \
	$(SUB_PATH)/base/android/apk_assets.cc \
	$(SUB_PATH)/base/android/application_status_listener.cc \
	$(SUB_PATH)/base/android/base_jni_onload.cc \
	$(SUB_PATH)/base/android/build_info.cc \
	$(SUB_PATH)/base/android/bundle_utils.cc \
	$(SUB_PATH)/base/android/callback_android.cc \
	$(SUB_PATH)/base/android/child_process_service.cc \
	$(SUB_PATH)/base/android/command_line_android.cc \
	$(SUB_PATH)/base/android/content_uri_utils.cc \
	$(SUB_PATH)/base/android/cpu_features.cc \
	$(SUB_PATH)/base/android/early_trace_event_binding.cc \
	$(SUB_PATH)/base/android/event_log.cc \
	$(SUB_PATH)/base/android/feature_list_jni.cc \
	$(SUB_PATH)/base/android/field_trial_list.cc \
	$(SUB_PATH)/base/android/important_file_writer_android.cc \
	$(SUB_PATH)/base/android/int_string_callback.cc \
	$(SUB_PATH)/base/android/java_exception_reporter.cc \
	$(SUB_PATH)/base/android/java_handler_thread.cc \
	$(SUB_PATH)/base/android/java_heap_dump_generator.cc \
	$(SUB_PATH)/base/android/java_runtime.cc \
	$(SUB_PATH)/base/android/jni_android.cc \
	$(SUB_PATH)/base/android/jni_array.cc \
	$(SUB_PATH)/base/android/jni_registrar.cc \
	$(SUB_PATH)/base/android/jni_string.cc \
	$(SUB_PATH)/base/android/jni_utils.cc \
	$(SUB_PATH)/base/android/jni_weak_ref.cc \
	$(SUB_PATH)/base/android/library_loader/anchor_functions.cc \
	$(SUB_PATH)/base/android/library_loader/library_loader_hooks.cc \
	$(SUB_PATH)/base/android/library_loader/library_prefetcher.cc \
	$(SUB_PATH)/base/android/library_loader/library_prefetcher_hooks.cc \
	$(SUB_PATH)/base/android/locale_utils.cc \
	$(SUB_PATH)/base/android/memory_pressure_listener_android.cc \
	$(SUB_PATH)/base/android/native_uma_recorder.cc \
	$(SUB_PATH)/base/android/path_service_android.cc \
	$(SUB_PATH)/base/android/path_utils.cc \
	$(SUB_PATH)/base/android/radio_utils.cc \
	$(SUB_PATH)/base/android/reached_addresses_bitset.cc \
	$(SUB_PATH)/base/android/reached_code_profiler.cc \
	$(SUB_PATH)/base/android/record_histogram.cc \
	$(SUB_PATH)/base/android/record_user_action.cc \
	$(SUB_PATH)/base/android/scoped_hardware_buffer_fence_sync.cc \
	$(SUB_PATH)/base/android/scoped_hardware_buffer_handle.cc \
	$(SUB_PATH)/base/android/scoped_java_ref.cc \
	$(SUB_PATH)/base/android/statistics_recorder_android.cc \
	$(SUB_PATH)/base/android/sys_utils.cc \
	$(SUB_PATH)/base/android/task_scheduler/post_task_android.cc \
	$(SUB_PATH)/base/android/task_scheduler/task_runner_android.cc \
	$(SUB_PATH)/base/android/time_utils.cc \
	$(SUB_PATH)/base/android/timezone_utils.cc \
	$(SUB_PATH)/base/android/trace_event_binding.cc \
	$(SUB_PATH)/base/android/unguessable_token_android.cc \
	$(SUB_PATH)/base/containers/flat_tree.cc \
	$(SUB_PATH)/base/containers/intrusive_heap.cc \
	$(SUB_PATH)/base/containers/linked_list.cc \
	$(SUB_PATH)/base/debug/activity_analyzer.cc \
	$(SUB_PATH)/base/debug/activity_tracker.cc \
	$(SUB_PATH)/base/debug/alias.cc \
	$(SUB_PATH)/base/debug/asan_invalid_access.cc \
	$(SUB_PATH)/base/debug/crash_logging.cc \
	$(SUB_PATH)/base/debug/debugger.cc \
	$(SUB_PATH)/base/debug/debugger_posix.cc \
	$(SUB_PATH)/base/debug/dump_without_crashing.cc \
	$(SUB_PATH)/base/debug/proc_maps_linux.cc \
	$(SUB_PATH)/base/debug/profiler.cc \
	$(SUB_PATH)/base/debug/stack_trace.cc \
	$(SUB_PATH)/base/debug/stack_trace_android.cc \
	$(SUB_PATH)/base/debug/task_trace.cc \
	$(SUB_PATH)/base/files/file.cc \
	$(SUB_PATH)/base/files/file_descriptor_watcher_posix.cc \
	$(SUB_PATH)/base/files/file_enumerator.cc \
	$(SUB_PATH)/base/files/file_enumerator_posix.cc \
	$(SUB_PATH)/base/files/file_path.cc \
	$(SUB_PATH)/base/files/file_path_constants.cc \
	$(SUB_PATH)/base/files/file_path_watcher.cc \
	$(SUB_PATH)/base/files/file_path_watcher_linux.cc \
	$(SUB_PATH)/base/files/file_posix.cc \
	$(SUB_PATH)/base/files/file_proxy.cc \
	$(SUB_PATH)/base/files/file_tracing.cc \
	$(SUB_PATH)/base/files/file_util.cc \
	$(SUB_PATH)/base/files/file_util_android.cc \
	$(SUB_PATH)/base/files/file_util_posix.cc \
	$(SUB_PATH)/base/files/important_file_writer.cc \
	$(SUB_PATH)/base/files/important_file_writer_cleaner.cc \
	$(SUB_PATH)/base/files/memory_mapped_file.cc \
	$(SUB_PATH)/base/files/memory_mapped_file_posix.cc \
	$(SUB_PATH)/base/files/scoped_file.cc \
	$(SUB_PATH)/base/files/scoped_file_android.cc \
	$(SUB_PATH)/base/files/scoped_temp_dir.cc \
	$(SUB_PATH)/base/hash/hash.cc \
	$(SUB_PATH)/base/hash/legacy_hash.cc \
	$(SUB_PATH)/base/hash/md5_boringssl.cc \
	$(SUB_PATH)/base/hash/sha1_boringssl.cc \
	$(SUB_PATH)/base/json/json_file_value_serializer.cc \
	$(SUB_PATH)/base/json/json_parser.cc \
	$(SUB_PATH)/base/json/json_reader.cc \
	$(SUB_PATH)/base/json/json_string_value_serializer.cc \
	$(SUB_PATH)/base/json/json_value_converter.cc \
	$(SUB_PATH)/base/json/json_writer.cc \
	$(SUB_PATH)/base/json/string_escape.cc \
	$(SUB_PATH)/base/memory/memory_pressure_listener.cc \
	$(SUB_PATH)/base/memory/memory_pressure_monitor.cc \
	$(SUB_PATH)/base/memory/platform_shared_memory_region.cc \
	$(SUB_PATH)/base/memory/platform_shared_memory_region_android.cc \
	$(SUB_PATH)/base/memory/read_only_shared_memory_region.cc \
	$(SUB_PATH)/base/memory/ref_counted.cc \
	$(SUB_PATH)/base/memory/ref_counted_memory.cc \
	$(SUB_PATH)/base/memory/shared_memory_mapping.cc \
	$(SUB_PATH)/base/memory/shared_memory_security_policy.cc \
	$(SUB_PATH)/base/memory/shared_memory_tracker.cc \
	$(SUB_PATH)/base/memory/unsafe_shared_memory_region.cc \
	$(SUB_PATH)/base/memory/weak_ptr.cc \
	$(SUB_PATH)/base/memory/writable_shared_memory_region.cc \
	$(SUB_PATH)/base/message_loop/message_pump.cc \
	$(SUB_PATH)/base/message_loop/message_pump_android.cc \
	$(SUB_PATH)/base/message_loop/message_pump_default.cc \
	$(SUB_PATH)/base/message_loop/message_pump_libevent.cc \
	$(SUB_PATH)/base/message_loop/watchable_io_message_pump_posix.cc \
	$(SUB_PATH)/base/message_loop/work_id_provider.cc \
	$(SUB_PATH)/base/metrics/bucket_ranges.cc \
	$(SUB_PATH)/base/metrics/crc32.cc \
	$(SUB_PATH)/base/metrics/dummy_histogram.cc \
	$(SUB_PATH)/base/metrics/field_trial.cc \
	$(SUB_PATH)/base/metrics/field_trial_param_associator.cc \
	$(SUB_PATH)/base/metrics/field_trial_params.cc \
	$(SUB_PATH)/base/metrics/histogram.cc \
	$(SUB_PATH)/base/metrics/histogram_base.cc \
	$(SUB_PATH)/base/metrics/histogram_delta_serialization.cc \
	$(SUB_PATH)/base/metrics/histogram_functions.cc \
	$(SUB_PATH)/base/metrics/histogram_samples.cc \
	$(SUB_PATH)/base/metrics/histogram_snapshot_manager.cc \
	$(SUB_PATH)/base/metrics/metrics_hashes.cc \
	$(SUB_PATH)/base/metrics/persistent_histogram_allocator.cc \
	$(SUB_PATH)/base/metrics/persistent_memory_allocator.cc \
	$(SUB_PATH)/base/metrics/persistent_sample_map.cc \
	$(SUB_PATH)/base/metrics/sample_map.cc \
	$(SUB_PATH)/base/metrics/sample_vector.cc \
	$(SUB_PATH)/base/metrics/single_sample_metrics.cc \
	$(SUB_PATH)/base/metrics/sparse_histogram.cc \
	$(SUB_PATH)/base/metrics/statistics_recorder.cc \
	$(SUB_PATH)/base/metrics/user_metrics.cc \
	$(SUB_PATH)/base/nix/mime_util_xdg.cc \
	$(SUB_PATH)/base/nix/xdg_util.cc \
	$(SUB_PATH)/base/posix/can_lower_nice_to.cc \
	$(SUB_PATH)/base/posix/file_descriptor_shuffle.cc \
	$(SUB_PATH)/base/posix/global_descriptors.cc \
	$(SUB_PATH)/base/posix/safe_strerror.cc \
	$(SUB_PATH)/base/posix/unix_domain_socket.cc \
	$(SUB_PATH)/base/power_monitor/power_monitor.cc \
	$(SUB_PATH)/base/power_monitor/power_monitor_device_source.cc \
	$(SUB_PATH)/base/power_monitor/power_monitor_device_source_android.cc \
	$(SUB_PATH)/base/power_monitor/power_monitor_source.cc \
	$(SUB_PATH)/base/process/environment_internal.cc \
	$(SUB_PATH)/base/process/internal_linux.cc \
	$(SUB_PATH)/base/process/kill.cc \
	$(SUB_PATH)/base/process/kill_posix.cc \
	$(SUB_PATH)/base/process/launch.cc \
	$(SUB_PATH)/base/process/launch_posix.cc \
	$(SUB_PATH)/base/process/memory.cc \
	$(SUB_PATH)/base/process/memory_linux.cc \
	$(SUB_PATH)/base/process/process_handle.cc \
	$(SUB_PATH)/base/process/process_handle_linux.cc \
	$(SUB_PATH)/base/process/process_handle_posix.cc \
	$(SUB_PATH)/base/process/process_iterator.cc \
	$(SUB_PATH)/base/process/process_iterator_linux.cc \
	$(SUB_PATH)/base/process/process_metrics.cc \
	$(SUB_PATH)/base/process/process_metrics_linux.cc \
	$(SUB_PATH)/base/process/process_metrics_posix.cc \
	$(SUB_PATH)/base/process/process_posix.cc \
	$(SUB_PATH)/base/strings/abseil_string_conversions.cc \
	$(SUB_PATH)/base/strings/escape.cc \
	$(SUB_PATH)/base/strings/latin1_string_conversions.cc \
	$(SUB_PATH)/base/strings/nullable_string16.cc \
	$(SUB_PATH)/base/strings/pattern.cc \
	$(SUB_PATH)/base/strings/safe_sprintf.cc \
	$(SUB_PATH)/base/strings/strcat.cc \
	$(SUB_PATH)/base/strings/string16.cc \
	$(SUB_PATH)/base/strings/string_number_conversions.cc \
	$(SUB_PATH)/base/strings/string_piece.cc \
	$(SUB_PATH)/base/strings/string_split.cc \
	$(SUB_PATH)/base/strings/string_util.cc \
	$(SUB_PATH)/base/strings/string_util_constants.cc \
	$(SUB_PATH)/base/strings/stringprintf.cc \
	$(SUB_PATH)/base/strings/sys_string_conversions_posix.cc \
	$(SUB_PATH)/base/strings/utf_offset_string_conversions.cc \
	$(SUB_PATH)/base/strings/utf_string_conversion_utils.cc \
	$(SUB_PATH)/base/strings/utf_string_conversions.cc \
	$(SUB_PATH)/base/synchronization/atomic_flag.cc \
	$(SUB_PATH)/base/synchronization/condition_variable_posix.cc \
	$(SUB_PATH)/base/synchronization/lock.cc \
	$(SUB_PATH)/base/synchronization/lock_impl_posix.cc \
	$(SUB_PATH)/base/synchronization/waitable_event_posix.cc \
	$(SUB_PATH)/base/synchronization/waitable_event_watcher_posix.cc \
	$(SUB_PATH)/base/system/sys_info_posix.cc \
	$(SUB_PATH)/base/task/cancelable_task_tracker.cc \
	$(SUB_PATH)/base/task/common/checked_lock_impl.cc \
	$(SUB_PATH)/base/task/common/operations_controller.cc \
	$(SUB_PATH)/base/task/common/scoped_defer_task_posting.cc \
	$(SUB_PATH)/base/task/common/task_annotator.cc \
	$(SUB_PATH)/base/task/current_thread.cc \
	$(SUB_PATH)/base/task/lazy_thread_pool_task_runner.cc \
	$(SUB_PATH)/base/task/post_job.cc \
	$(SUB_PATH)/base/task/post_task.cc \
	$(SUB_PATH)/base/task/scoped_set_task_priority_for_current_thread.cc \
	$(SUB_PATH)/base/task/sequence_manager/associated_thread_id.cc \
	$(SUB_PATH)/base/task/sequence_manager/atomic_flag_set.cc \
	$(SUB_PATH)/base/task/sequence_manager/enqueue_order_generator.cc \
	$(SUB_PATH)/base/task/sequence_manager/lazy_now.cc \
	$(SUB_PATH)/base/task/sequence_manager/real_time_domain.cc \
	$(SUB_PATH)/base/task/sequence_manager/sequence_manager.cc \
	$(SUB_PATH)/base/task/sequence_manager/sequence_manager_impl.cc \
	$(SUB_PATH)/base/task/sequence_manager/task_queue.cc \
	$(SUB_PATH)/base/task/sequence_manager/task_queue_impl.cc \
	$(SUB_PATH)/base/task/sequence_manager/task_queue_selector.cc \
	$(SUB_PATH)/base/task/sequence_manager/tasks.cc \
	$(SUB_PATH)/base/task/sequence_manager/thread_controller.cc \
	$(SUB_PATH)/base/task/sequence_manager/thread_controller_power_monitor.cc \
	$(SUB_PATH)/base/task/sequence_manager/thread_controller_with_message_pump_impl.cc \
	$(SUB_PATH)/base/task/sequence_manager/time_domain.cc \
	$(SUB_PATH)/base/task/sequence_manager/work_deduplicator.cc \
	$(SUB_PATH)/base/task/sequence_manager/work_queue.cc \
	$(SUB_PATH)/base/task/sequence_manager/work_queue_sets.cc \
	$(SUB_PATH)/base/task/simple_task_executor.cc \
	$(SUB_PATH)/base/task/single_thread_task_executor.cc \
	$(SUB_PATH)/base/task/task_executor.cc \
	$(SUB_PATH)/base/task/task_features.cc \
	$(SUB_PATH)/base/task/task_traits.cc \
	$(SUB_PATH)/base/task/thread_pool.cc \
	$(SUB_PATH)/base/task/thread_pool/delayed_task_manager.cc \
	$(SUB_PATH)/base/task/thread_pool/environment_config.cc \
	$(SUB_PATH)/base/task/thread_pool/initialization_util.cc \
	$(SUB_PATH)/base/task/thread_pool/job_task_source.cc \
	$(SUB_PATH)/base/task/thread_pool/pooled_parallel_task_runner.cc \
	$(SUB_PATH)/base/task/thread_pool/pooled_sequenced_task_runner.cc \
	$(SUB_PATH)/base/task/thread_pool/pooled_single_thread_task_runner_manager.cc \
	$(SUB_PATH)/base/task/thread_pool/pooled_task_runner_delegate.cc \
	$(SUB_PATH)/base/task/thread_pool/priority_queue.cc \
	$(SUB_PATH)/base/task/thread_pool/sequence.cc \
	$(SUB_PATH)/base/task/thread_pool/service_thread.cc \
	$(SUB_PATH)/base/task/thread_pool/task.cc \
	$(SUB_PATH)/base/task/thread_pool/task_source.cc \
	$(SUB_PATH)/base/task/thread_pool/task_source_sort_key.cc \
	$(SUB_PATH)/base/task/thread_pool/task_tracker.cc \
	$(SUB_PATH)/base/task/thread_pool/task_tracker_posix.cc \
	$(SUB_PATH)/base/task/thread_pool/thread_group.cc \
	$(SUB_PATH)/base/task/thread_pool/thread_group_impl.cc \
	$(SUB_PATH)/base/task/thread_pool/thread_group_native.cc \
	$(SUB_PATH)/base/task/thread_pool/thread_pool_impl.cc \
	$(SUB_PATH)/base/task/thread_pool/thread_pool_instance.cc \
	$(SUB_PATH)/base/task/thread_pool/worker_thread.cc \
	$(SUB_PATH)/base/task/thread_pool/worker_thread_stack.cc \
	$(SUB_PATH)/base/third_party/cityhash/city.cc \
	$(SUB_PATH)/base/third_party/cityhash_v103/src/city_v103.cc \
	$(SUB_PATH)/base/third_party/double_conversion/double-conversion/bignum-dtoa.cc \
	$(SUB_PATH)/base/third_party/double_conversion/double-conversion/bignum.cc \
	$(SUB_PATH)/base/third_party/double_conversion/double-conversion/cached-powers.cc \
	$(SUB_PATH)/base/third_party/double_conversion/double-conversion/double-to-string.cc \
	$(SUB_PATH)/base/third_party/double_conversion/double-conversion/fast-dtoa.cc \
	$(SUB_PATH)/base/third_party/double_conversion/double-conversion/fixed-dtoa.cc \
	$(SUB_PATH)/base/third_party/double_conversion/double-conversion/string-to-double.cc \
	$(SUB_PATH)/base/third_party/double_conversion/double-conversion/strtod.cc \
	$(SUB_PATH)/base/third_party/nspr/prtime.cc \
	$(SUB_PATH)/base/third_party/superfasthash/superfasthash.c \
	$(SUB_PATH)/base/third_party/xdg_mime/xdgmime.c \
    $(SUB_PATH)/base/third_party/xdg_mime/xdgmimealias.c \
    $(SUB_PATH)/base/third_party/xdg_mime/xdgmimecache.c \
    $(SUB_PATH)/base/third_party/xdg_mime/xdgmimeglob.c \
    $(SUB_PATH)/base/third_party/xdg_mime/xdgmimeicon.c \
    $(SUB_PATH)/base/third_party/xdg_mime/xdgmimeint.c \
    $(SUB_PATH)/base/third_party/xdg_mime/xdgmimemagic.c \
    $(SUB_PATH)/base/third_party/xdg_mime/xdgmimeparent.c \
	$(SUB_PATH)/base/third_party/xdg_user_dirs/xdg_user_dir_lookup.cc \
	$(SUB_PATH)/base/threading/hang_watcher.cc \
	$(SUB_PATH)/base/threading/platform_thread.cc \
	$(SUB_PATH)/base/threading/platform_thread_android.cc \
	$(SUB_PATH)/base/threading/platform_thread_internal_posix.cc \
	$(SUB_PATH)/base/threading/platform_thread_posix.cc \
	$(SUB_PATH)/base/threading/post_task_and_reply_impl.cc \
	$(SUB_PATH)/base/threading/scoped_blocking_call.cc \
	$(SUB_PATH)/base/threading/scoped_blocking_call_internal.cc \
	$(SUB_PATH)/base/threading/sequence_local_storage_map.cc \
	$(SUB_PATH)/base/threading/sequence_local_storage_slot.cc \
	$(SUB_PATH)/base/threading/sequenced_task_runner_handle.cc \
	$(SUB_PATH)/base/threading/simple_thread.cc \
	$(SUB_PATH)/base/threading/thread.cc \
	$(SUB_PATH)/base/threading/thread_checker_impl.cc \
	$(SUB_PATH)/base/threading/thread_collision_warner.cc \
	$(SUB_PATH)/base/threading/thread_id_name_manager.cc \
	$(SUB_PATH)/base/threading/thread_local_storage.cc \
	$(SUB_PATH)/base/threading/thread_local_storage_posix.cc \
	$(SUB_PATH)/base/threading/thread_restrictions.cc \
	$(SUB_PATH)/base/threading/thread_task_runner_handle.cc \
	$(SUB_PATH)/base/threading/watchdog.cc \
	$(SUB_PATH)/base/time/clock.cc \
	$(SUB_PATH)/base/time/default_clock.cc \
	$(SUB_PATH)/base/time/default_tick_clock.cc \
	$(SUB_PATH)/base/time/tick_clock.cc \
	$(SUB_PATH)/base/time/time.cc \
	$(SUB_PATH)/base/time/time_android.cc \
	$(SUB_PATH)/base/time/time_conversion_posix.cc \
	$(SUB_PATH)/base/time/time_exploded_posix.cc \
	$(SUB_PATH)/base/time/time_now_posix.cc \
	$(SUB_PATH)/base/time/time_override.cc \
	$(SUB_PATH)/base/time/time_to_iso8601.cc \
	$(SUB_PATH)/base/timer/elapsed_timer.cc \
	$(SUB_PATH)/base/timer/hi_res_timer_manager_posix.cc \
	$(SUB_PATH)/base/timer/timer.cc \
	$(SUB_PATH)/base/trace_event/heap_profiler_allocation_context.cc \
	$(SUB_PATH)/base/trace_event/heap_profiler_allocation_context_tracker.cc \
	$(SUB_PATH)/base/trace_event/memory_allocator_dump_guid.cc \
	$(SUB_PATH)/base/trace_event/trace_event_stub.cc \
	$(SUB_PATH)/base/trace_event/trace_id_helper.cc \
	$(SUB_PATH)/base/at_exit.cc \
	$(SUB_PATH)/base/barrier_closure.cc \
	$(SUB_PATH)/base/base_paths.cc \
	$(SUB_PATH)/base/base_paths_android.cc \
	$(SUB_PATH)/base/base_switches.cc \
	$(SUB_PATH)/base/base64.cc \
	$(SUB_PATH)/base/base64url.cc \
	$(SUB_PATH)/base/big_endian.cc \
	$(SUB_PATH)/base/build_time.cc \
	$(SUB_PATH)/base/callback_helpers.cc \
	$(SUB_PATH)/base/callback_internal.cc \
	$(SUB_PATH)/base/callback_list.cc \
	$(SUB_PATH)/base/check.cc \
	$(SUB_PATH)/base/check_op.cc \
	$(SUB_PATH)/base/command_line.cc \
	$(SUB_PATH)/base/cpu.cc \
	$(SUB_PATH)/base/deferred_sequenced_task_runner.cc \
	$(SUB_PATH)/base/environment.cc \
	$(SUB_PATH)/base/feature_list.cc \
	$(SUB_PATH)/base/file_descriptor_store.cc \
	$(SUB_PATH)/base/guid.cc \
	$(SUB_PATH)/base/i18n/i18n_constants.cc \
	$(SUB_PATH)/base/lazy_instance_helpers.cc \
	$(SUB_PATH)/base/linux_util.cc \
	$(SUB_PATH)/base/location.cc \
	$(SUB_PATH)/base/logging.cc \
	$(SUB_PATH)/base/native_library.cc \
	$(SUB_PATH)/base/native_library_posix.cc \
	$(SUB_PATH)/base/observer_list_internal.cc \
	$(SUB_PATH)/base/observer_list_types.cc \
	$(SUB_PATH)/base/observer_list_threadsafe.cc \
	$(SUB_PATH)/base/os_compat_android.cc \
	$(SUB_PATH)/base/path_service.cc \
	$(SUB_PATH)/base/pending_task.cc \
	$(SUB_PATH)/base/pickle.cc \
	$(SUB_PATH)/base/rand_util.cc \
	$(SUB_PATH)/base/rand_util_posix.cc \
	$(SUB_PATH)/base/run_loop.cc \
	$(SUB_PATH)/base/scoped_native_library.cc \
	$(SUB_PATH)/base/sequence_checker_impl.cc \
	$(SUB_PATH)/base/sequence_token.cc \
	$(SUB_PATH)/base/sequenced_task_runner.cc \
	$(SUB_PATH)/base/supports_user_data.cc \
	$(SUB_PATH)/base/sync_socket.cc \
	$(SUB_PATH)/base/sync_socket_posix.cc \
	$(SUB_PATH)/base/syslog_logging.cc \
	$(SUB_PATH)/base/task_runner.cc \
	$(SUB_PATH)/base/token.cc \
	$(SUB_PATH)/base/unguessable_token.cc \
	$(SUB_PATH)/base/value_iterators.cc \
	$(SUB_PATH)/base/values.cc \
	$(SUB_PATH)/base/version.cc \
	$(SUB_PATH)/base/vlog.cc \
	$(SUB_PATH)/crypto/aead.cc \
    $(SUB_PATH)/crypto/ec_private_key.cc \
    $(SUB_PATH)/crypto/ec_signature_creator.cc \
    $(SUB_PATH)/crypto/ec_signature_creator_impl.cc \
    $(SUB_PATH)/crypto/encryptor.cc \
    $(SUB_PATH)/crypto/hkdf.cc \
    $(SUB_PATH)/crypto/hmac.cc \
    $(SUB_PATH)/crypto/openssl_util.cc \
    $(SUB_PATH)/crypto/p224.cc \
    $(SUB_PATH)/crypto/p224_spake.cc \
    $(SUB_PATH)/crypto/random.cc \
    $(SUB_PATH)/crypto/rsa_private_key.cc \
    $(SUB_PATH)/crypto/secure_hash.cc \
    $(SUB_PATH)/crypto/secure_util.cc \
    $(SUB_PATH)/crypto/sha2.cc \
    $(SUB_PATH)/crypto/signature_creator.cc \
    $(SUB_PATH)/crypto/signature_verifier.cc \
    $(SUB_PATH)/crypto/symmetric_key.cc \
	$(SUB_PATH)/net/android/cellular_signal_strength.cc \
	$(SUB_PATH)/net/android/cert_verify_result_android.cc \
	$(SUB_PATH)/net/android/gurl_utils.cc \
	$(SUB_PATH)/net/android/keystore.cc \
	$(SUB_PATH)/net/android/network_activation_request.cc \
	$(SUB_PATH)/net/android/network_change_notifier_android.cc \
	$(SUB_PATH)/net/android/network_change_notifier_delegate_android.cc \
	$(SUB_PATH)/net/android/network_change_notifier_factory_android.cc \
	$(SUB_PATH)/net/android/network_library.cc \
	$(SUB_PATH)/net/android/traffic_stats.cc \
	$(SUB_PATH)/net/base/address_family.cc \
	$(SUB_PATH)/net/base/address_list.cc \
	$(SUB_PATH)/net/base/address_tracker_linux.cc \
	$(SUB_PATH)/net/base/auth.cc \
	$(SUB_PATH)/net/base/backoff_entry.cc \
	$(SUB_PATH)/net/base/backoff_entry_serializer.cc \
	$(SUB_PATH)/net/base/chunked_upload_data_stream.cc \
	$(SUB_PATH)/net/base/data_url.cc \
	$(SUB_PATH)/net/base/datagram_buffer.cc \
	$(SUB_PATH)/net/base/elements_upload_data_stream.cc \
	$(SUB_PATH)/net/base/escape.cc \
	$(SUB_PATH)/net/base/features.cc \
	$(SUB_PATH)/net/base/file_stream.cc \
	$(SUB_PATH)/net/base/file_stream_context.cc \
	$(SUB_PATH)/net/base/file_stream_context_posix.cc \
	$(SUB_PATH)/net/base/filename_util.cc \
	$(SUB_PATH)/net/base/filename_util_internal.cc \
	$(SUB_PATH)/net/base/hash_value.cc \
	$(SUB_PATH)/net/base/hex_utils.cc \
	$(SUB_PATH)/net/base/host_mapping_rules.cc \
	$(SUB_PATH)/net/base/host_port_pair.cc \
	$(SUB_PATH)/net/base/io_buffer.cc \
	$(SUB_PATH)/net/base/ip_address.cc \
	$(SUB_PATH)/net/base/ip_endpoint.cc \
	$(SUB_PATH)/net/base/isolation_info.cc \
	$(SUB_PATH)/net/base/load_timing_info.cc \
	$(SUB_PATH)/net/base/logging_network_change_observer.cc \
	$(SUB_PATH)/net/base/lookup_string_in_fixed_set.cc \
	$(SUB_PATH)/net/base/mime_sniffer.cc \
	$(SUB_PATH)/net/base/mime_util.cc \
	$(SUB_PATH)/net/base/net_errors.cc \
	$(SUB_PATH)/net/base/net_errors_posix.cc \
	$(SUB_PATH)/net/base/net_module.cc \
	$(SUB_PATH)/net/base/net_string_util_icu.cc \
	$(SUB_PATH)/net/base/network_activity_monitor.cc \
	$(SUB_PATH)/net/base/network_interfaces.cc \
	$(SUB_PATH)/net/base/network_isolation_key.cc \
	$(SUB_PATH)/net/base/network_change_notifier.cc \
	$(SUB_PATH)/net/base/network_change_notifier_posix.cc \
	$(SUB_PATH)/net/base/network_delegate.cc \
	$(SUB_PATH)/net/base/network_delegate_impl.cc \
	$(SUB_PATH)/net/base/network_interfaces_getifaddrs.cc \
	$(SUB_PATH)/net/base/network_interfaces_linux.cc \
	$(SUB_PATH)/net/base/network_interfaces_posix.cc \
	$(SUB_PATH)/net/base/parse_number.cc \
	$(SUB_PATH)/net/base/platform_mime_util_linux.cc \
	$(SUB_PATH)/net/base/port_util.cc \
	$(SUB_PATH)/net/base/prioritized_dispatcher.cc \
	$(SUB_PATH)/net/base/privacy_mode.cc \
	$(SUB_PATH)/net/base/proxy_server.cc \
	$(SUB_PATH)/net/base/registry_controlled_domains/registry_controlled_domain.cc \
	$(SUB_PATH)/net/base/request_priority.cc \
	$(SUB_PATH)/net/base/scheme_host_port_matcher.cc \
	$(SUB_PATH)/net/base/scheme_host_port_matcher_rule.cc \
	$(SUB_PATH)/net/base/schemeful_site.cc \
	$(SUB_PATH)/net/base/sockaddr_storage.cc \
	$(SUB_PATH)/net/base/test_data_stream.cc \
	$(SUB_PATH)/net/base/transport_info.cc \
	$(SUB_PATH)/net/base/upload_bytes_element_reader.cc \
	$(SUB_PATH)/net/base/upload_data_stream.cc \
	$(SUB_PATH)/net/base/upload_element_reader.cc \
	$(SUB_PATH)/net/base/upload_file_element_reader.cc \
	$(SUB_PATH)/net/base/url_util.cc \
	$(SUB_PATH)/net/cert/internal/cert_error_id.cc \
	$(SUB_PATH)/net/cert/internal/cert_error_params.cc \
	$(SUB_PATH)/net/cert/internal/cert_errors.cc \
	$(SUB_PATH)/net/cert/internal/cert_issuer_source_aia.cc \
	$(SUB_PATH)/net/cert/internal/cert_issuer_source_static.cc \
	$(SUB_PATH)/net/cert/internal/certificate_policies.cc \
	$(SUB_PATH)/net/cert/internal/common_cert_errors.cc \
	$(SUB_PATH)/net/cert/internal/crl.cc \
	$(SUB_PATH)/net/cert/internal/extended_key_usage.cc \
	$(SUB_PATH)/net/cert/internal/general_names.cc \
	$(SUB_PATH)/net/cert/internal/name_constraints.cc \
	$(SUB_PATH)/net/cert/internal/ocsp.cc \
	$(SUB_PATH)/net/cert/internal/parse_certificate.cc \
	$(SUB_PATH)/net/cert/internal/parse_name.cc \
	$(SUB_PATH)/net/cert/internal/parsed_certificate.cc \
	$(SUB_PATH)/net/cert/internal/path_builder.cc \
	$(SUB_PATH)/net/cert/internal/revocation_checker.cc \
	$(SUB_PATH)/net/cert/internal/revocation_util.cc \
	$(SUB_PATH)/net/cert/internal/signature_algorithm.cc \
	$(SUB_PATH)/net/cert/internal/simple_path_builder_delegate.cc \
	$(SUB_PATH)/net/cert/internal/system_trust_store.cc \
	$(SUB_PATH)/net/cert/internal/trust_store.cc \
	$(SUB_PATH)/net/cert/internal/trust_store_collection.cc \
	$(SUB_PATH)/net/cert/internal/trust_store_in_memory.cc \
	$(SUB_PATH)/net/cert/internal/verify_certificate_chain.cc \
	$(SUB_PATH)/net/cert/internal/verify_name_match.cc \
	$(SUB_PATH)/net/cert/internal/verify_signed_data.cc \
	$(SUB_PATH)/net/cert/asn1_util.cc \
	$(SUB_PATH)/net/cert/caching_cert_verifier.cc \
	$(SUB_PATH)/net/cert/cert_database.cc \
	$(SUB_PATH)/net/cert/cert_status_flags.cc \
	$(SUB_PATH)/net/cert/cert_verifier.cc \
	$(SUB_PATH)/net/cert/cert_verify_proc.cc \
	$(SUB_PATH)/net/cert/cert_verify_proc_android.cc \
	$(SUB_PATH)/net/cert/cert_verify_proc_builtin.cc \
	$(SUB_PATH)/net/cert/cert_verify_result.cc \
	$(SUB_PATH)/net/cert/coalescing_cert_verifier.cc \
	$(SUB_PATH)/net/cert/crl_set.cc \
	$(SUB_PATH)/net/cert/ct_log_response_parser.cc \
	$(SUB_PATH)/net/cert/ct_log_verifier.cc \
	$(SUB_PATH)/net/cert/ct_log_verifier_util.cc \
	$(SUB_PATH)/net/cert/ct_policy_enforcer.cc \
	$(SUB_PATH)/net/cert/ct_objects_extractor.cc \
	$(SUB_PATH)/net/cert/ct_sct_to_string.cc \
	$(SUB_PATH)/net/cert/ct_serialization.cc \
	$(SUB_PATH)/net/cert/ct_signed_certificate_timestamp_log_param.cc \
	$(SUB_PATH)/net/cert/do_nothing_ct_verifier.cc \
	$(SUB_PATH)/net/cert/ev_root_ca_metadata.cc \
	$(SUB_PATH)/net/cert/known_roots.cc \
	$(SUB_PATH)/net/cert/merkle_audit_proof.cc \
	$(SUB_PATH)/net/cert/merkle_consistency_proof.cc \
	$(SUB_PATH)/net/cert/merkle_tree_leaf.cc \
	$(SUB_PATH)/net/cert/mock_cert_verifier.cc \
	$(SUB_PATH)/net/cert/multi_log_ct_verifier.cc \
	$(SUB_PATH)/net/cert/multi_threaded_cert_verifier.cc \
	$(SUB_PATH)/net/cert/ocsp_verify_result.cc \
	$(SUB_PATH)/net/cert/pem.cc \
	$(SUB_PATH)/net/cert/sct_status_flags.cc \
	$(SUB_PATH)/net/cert/signed_certificate_timestamp.cc \
	$(SUB_PATH)/net/cert/signed_certificate_timestamp_and_status.cc \
	$(SUB_PATH)/net/cert/signed_tree_head.cc \
	$(SUB_PATH)/net/cert/symantec_certs.cc \
	$(SUB_PATH)/net/cert/test_root_certs.cc \
	$(SUB_PATH)/net/cert/test_root_certs_android.cc \
	$(SUB_PATH)/net/cert/x509_cert_types.cc \
	$(SUB_PATH)/net/cert/x509_certificate.cc \
	$(SUB_PATH)/net/cert/x509_certificate_net_log_param.cc \
	$(SUB_PATH)/net/cert/x509_util.cc \
	$(SUB_PATH)/net/cert/x509_util_android.cc \
	$(SUB_PATH)/net/der/encode_values.cc \
	$(SUB_PATH)/net/der/input.cc \
	$(SUB_PATH)/net/der/parse_values.cc \
	$(SUB_PATH)/net/der/parser.cc \
	$(SUB_PATH)/net/der/tag.cc \
	$(SUB_PATH)/net/dns/public/dns_config_overrides.cc \
	$(SUB_PATH)/net/dns/public/dns_over_https_server_config.cc \
	$(SUB_PATH)/net/dns/public/dns_query_type.cc \
	$(SUB_PATH)/net/dns/public/doh_provider_entry.cc \
	$(SUB_PATH)/net/dns/public/resolve_error_info.cc \
	$(SUB_PATH)/net/dns/public/util.cc \
	$(SUB_PATH)/net/dns/address_info.cc \
	$(SUB_PATH)/net/dns/address_sorter_posix.cc \
	$(SUB_PATH)/net/dns/context_host_resolver.cc \
	$(SUB_PATH)/net/dns/dns_client.cc \
	$(SUB_PATH)/net/dns/dns_config.cc \
	$(SUB_PATH)/net/dns/dns_config_service.cc \
	$(SUB_PATH)/net/dns/dns_config_service_posix.cc \
	$(SUB_PATH)/net/dns/dns_hosts.cc \
	$(SUB_PATH)/net/dns/dns_query.cc \
	$(SUB_PATH)/net/dns/dns_reloader.cc \
	$(SUB_PATH)/net/dns/dns_response.cc \
	$(SUB_PATH)/net/dns/dns_response_result_extractor.cc \
	$(SUB_PATH)/net/dns/dns_server_iterator.cc \
	$(SUB_PATH)/net/dns/dns_session.cc \
	$(SUB_PATH)/net/dns/dns_socket_allocator.cc \
	$(SUB_PATH)/net/dns/dns_transaction.cc \
	$(SUB_PATH)/net/dns/dns_udp_tracker.cc \
	$(SUB_PATH)/net/dns/dns_util.cc \
	$(SUB_PATH)/net/dns/host_cache.cc \
	$(SUB_PATH)/net/dns/host_resolver.cc \
	$(SUB_PATH)/net/dns/host_resolver_manager.cc \
	$(SUB_PATH)/net/dns/host_resolver_mdns_listener_impl.cc \
	$(SUB_PATH)/net/dns/host_resolver_mdns_task.cc \
	$(SUB_PATH)/net/dns/host_resolver_proc.cc \
	$(SUB_PATH)/net/dns/https_record_rdata.cc \
	$(SUB_PATH)/net/dns/httpssvc_metrics.cc \
	$(SUB_PATH)/net/dns/mapped_host_resolver.cc \
	$(SUB_PATH)/net/dns/mdns_cache.cc \
	$(SUB_PATH)/net/dns/mdns_client.cc \
	$(SUB_PATH)/net/dns/mdns_client_impl.cc \
	$(SUB_PATH)/net/dns/record_parsed.cc \
	$(SUB_PATH)/net/dns/record_rdata.cc \
	$(SUB_PATH)/net/dns/resolve_context.cc \
	$(SUB_PATH)/net/dns/serial_worker.cc \
	$(SUB_PATH)/net/dns/system_dns_config_change_notifier.cc \
	$(SUB_PATH)/net/extras/preload_data/decoder.cc \
	$(SUB_PATH)/net/filter/brotli_source_stream_disabled.cc \
	$(SUB_PATH)/net/filter/filter_source_stream.cc \
	$(SUB_PATH)/net/filter/gzip_header.cc \
	$(SUB_PATH)/net/filter/gzip_source_stream.cc \
	$(SUB_PATH)/net/filter/source_stream.cc \
	$(SUB_PATH)/net/http/alternative_service.cc \
	$(SUB_PATH)/net/http/bidirectional_stream.cc \
	$(SUB_PATH)/net/http/bidirectional_stream_impl.cc \
	$(SUB_PATH)/net/http/bidirectional_stream_request_info.cc \
	$(SUB_PATH)/net/http/broken_alternative_services.cc \
	$(SUB_PATH)/net/http/http_auth.cc \
	$(SUB_PATH)/net/http/http_auth_cache.cc \
	$(SUB_PATH)/net/http/http_auth_challenge_tokenizer.cc \
	$(SUB_PATH)/net/http/http_auth_controller.cc \
	$(SUB_PATH)/net/http/http_auth_filter.cc \
	$(SUB_PATH)/net/http/http_auth_handler.cc \
	$(SUB_PATH)/net/http/http_auth_handler_basic.cc \
	$(SUB_PATH)/net/http/http_auth_handler_digest.cc \
	$(SUB_PATH)/net/http/http_auth_handler_factory.cc \
	$(SUB_PATH)/net/http/http_auth_handler_ntlm.cc \
	$(SUB_PATH)/net/http/http_auth_handler_ntlm_portable.cc \
	$(SUB_PATH)/net/http/http_auth_multi_round_parse.cc \
	$(SUB_PATH)/net/http/http_auth_ntlm_mechanism.cc \
	$(SUB_PATH)/net/http/http_auth_preferences.cc \
	$(SUB_PATH)/net/http/http_auth_scheme.cc \
	$(SUB_PATH)/net/http/http_basic_state.cc \
	$(SUB_PATH)/net/http/http_basic_stream.cc \
	$(SUB_PATH)/net/http/http_byte_range.cc \
	$(SUB_PATH)/net/http/http_cache.cc \
	$(SUB_PATH)/net/http/http_cache_lookup_manager.cc \
	$(SUB_PATH)/net/http/http_cache_transaction.cc \
	$(SUB_PATH)/net/http/http_chunked_decoder.cc \
	$(SUB_PATH)/net/http/http_content_disposition.cc \
	$(SUB_PATH)/net/http/http_log_util.cc \
	$(SUB_PATH)/net/http/http_network_layer.cc \
	$(SUB_PATH)/net/http/http_network_session.cc \
	$(SUB_PATH)/net/http/http_network_session_peer.cc \
	$(SUB_PATH)/net/http/http_network_transaction.cc \
	$(SUB_PATH)/net/http/http_proxy_client_socket.cc \
	$(SUB_PATH)/net/http/http_proxy_connect_job.cc \
	$(SUB_PATH)/net/http/http_raw_request_headers.cc \
	$(SUB_PATH)/net/http/http_request_headers.cc \
	$(SUB_PATH)/net/http/http_request_info.cc \
	$(SUB_PATH)/net/http/http_response_body_drainer.cc \
	$(SUB_PATH)/net/http/http_response_headers.cc \
	$(SUB_PATH)/net/http/http_response_info.cc \
	$(SUB_PATH)/net/http/http_security_headers.cc \
	$(SUB_PATH)/net/http/http_server_properties.cc \
	$(SUB_PATH)/net/http/http_server_properties_manager.cc \
	$(SUB_PATH)/net/http/http_status_code.cc \
	$(SUB_PATH)/net/http/http_stream_factory.cc \
	$(SUB_PATH)/net/http/http_stream_factory_job.cc \
	$(SUB_PATH)/net/http/http_stream_factory_job_controller.cc \
	$(SUB_PATH)/net/http/http_stream_parser.cc \
	$(SUB_PATH)/net/http/http_stream_request.cc \
	$(SUB_PATH)/net/http/http_util.cc \
	$(SUB_PATH)/net/http/http_vary_data.cc \
	$(SUB_PATH)/net/http/partial_data.cc \
	$(SUB_PATH)/net/http/proxy_fallback.cc \
	$(SUB_PATH)/net/http/proxy_client_socket.cc \
	$(SUB_PATH)/net/http/transport_security_persister.cc \
	$(SUB_PATH)/net/http/transport_security_state.cc \
	$(SUB_PATH)/net/http/transport_security_state_source.cc \
	$(SUB_PATH)/net/http/url_security_manager.cc \
	$(SUB_PATH)/net/http/url_security_manager_posix.cc \
	$(SUB_PATH)/net/http/webfonts_histogram.cc \
	$(SUB_PATH)/net/log/file_net_log_observer.cc \
	$(SUB_PATH)/net/log/net_log.cc \
	$(SUB_PATH)/net/log/net_log_capture_mode.cc \
	$(SUB_PATH)/net/log/net_log_entry.cc \
	$(SUB_PATH)/net/log/net_log_source.cc \
	$(SUB_PATH)/net/log/net_log_util.cc \
	$(SUB_PATH)/net/log/net_log_values.cc \
	$(SUB_PATH)/net/log/net_log_with_source.cc \
	$(SUB_PATH)/net/log/trace_net_log_observer.cc \
	$(SUB_PATH)/net/nqe/proto/network_id_proto.pb.cc \
	$(SUB_PATH)/net/nqe/cached_network_quality.cc \
	$(SUB_PATH)/net/nqe/connectivity_monitor.cc \
	$(SUB_PATH)/net/nqe/effective_connection_type.cc \
	$(SUB_PATH)/net/nqe/event_creator.cc \
	$(SUB_PATH)/net/nqe/network_congestion_analyzer.cc \
	$(SUB_PATH)/net/nqe/network_id.cc \
	$(SUB_PATH)/net/nqe/network_qualities_prefs_manager.cc \
	$(SUB_PATH)/net/nqe/network_quality.cc \
	$(SUB_PATH)/net/nqe/network_quality_estimator.cc \
	$(SUB_PATH)/net/nqe/network_quality_estimator_params.cc \
	$(SUB_PATH)/net/nqe/network_quality_estimator_util.cc \
	$(SUB_PATH)/net/nqe/network_quality_observation.cc \
	$(SUB_PATH)/net/nqe/network_quality_store.cc \
	$(SUB_PATH)/net/nqe/observation_buffer.cc \
	$(SUB_PATH)/net/nqe/socket_watcher.cc \
	$(SUB_PATH)/net/nqe/socket_watcher_factory.cc \
	$(SUB_PATH)/net/nqe/throughput_analyzer.cc \
	$(SUB_PATH)/net/ntlm/ntlm.cc \
	$(SUB_PATH)/net/ntlm/ntlm_buffer_reader.cc \
	$(SUB_PATH)/net/ntlm/ntlm_buffer_writer.cc \
	$(SUB_PATH)/net/ntlm/ntlm_client.cc \
	$(SUB_PATH)/net/ntlm/ntlm_constants.cc \
	$(SUB_PATH)/net/proxy_resolution/configured_proxy_resolution_request.cc \
	$(SUB_PATH)/net/proxy_resolution/configured_proxy_resolution_service.cc \
	$(SUB_PATH)/net/proxy_resolution/dhcp_pac_file_fetcher.cc \
	$(SUB_PATH)/net/proxy_resolution/network_delegate_error_observer.cc \
	$(SUB_PATH)/net/proxy_resolution/pac_file_data.cc \
	$(SUB_PATH)/net/proxy_resolution/pac_file_decider.cc \
	$(SUB_PATH)/net/proxy_resolution/pac_file_fetcher_impl.cc \
	$(SUB_PATH)/net/proxy_resolution/polling_proxy_config_service.cc \
	$(SUB_PATH)/net/proxy_resolution/proxy_bypass_rules.cc \
	$(SUB_PATH)/net/proxy_resolution/proxy_config.cc \
	$(SUB_PATH)/net/proxy_resolution/proxy_config_service_fixed.cc \
	$(SUB_PATH)/net/proxy_resolution/proxy_config_with_annotation.cc \
	$(SUB_PATH)/net/proxy_resolution/proxy_info.cc \
	$(SUB_PATH)/net/proxy_resolution/proxy_list.cc \
	$(SUB_PATH)/net/proxy_resolution/proxy_resolver_factory.cc \
	$(SUB_PATH)/net/server/http_connection.cc \
	$(SUB_PATH)/net/server/http_server.cc \
	$(SUB_PATH)/net/server/http_server_request_info.cc \
	$(SUB_PATH)/net/server/http_server_response_info.cc \
	$(SUB_PATH)/net/socket/client_socket_factory.cc \
	$(SUB_PATH)/net/socket/client_socket_handle.cc \
	$(SUB_PATH)/net/socket/client_socket_pool.cc \
	$(SUB_PATH)/net/socket/client_socket_pool_manager.cc \
	$(SUB_PATH)/net/socket/client_socket_pool_manager_impl.cc \
	$(SUB_PATH)/net/socket/connect_job.cc \
	$(SUB_PATH)/net/socket/next_proto.cc \
	$(SUB_PATH)/net/socket/server_socket.cc \
	$(SUB_PATH)/net/socket/socket.cc \
	$(SUB_PATH)/net/socket/socket_bio_adapter.cc \
	$(SUB_PATH)/net/socket/socket_descriptor.cc \
	$(SUB_PATH)/net/socket/socket_net_log_params.cc \
	$(SUB_PATH)/net/socket/socket_options.cc \
	$(SUB_PATH)/net/socket/socket_posix.cc \
	$(SUB_PATH)/net/socket/socket_tag.cc \
	$(SUB_PATH)/net/socket/socks5_client_socket.cc \
	$(SUB_PATH)/net/socket/socks_connect_job.cc \
	$(SUB_PATH)/net/socket/socks_client_socket.cc \
	$(SUB_PATH)/net/socket/stream_socket.cc \
	$(SUB_PATH)/net/socket/ssl_client_socket.cc \
	$(SUB_PATH)/net/socket/ssl_client_socket_impl.cc \
	$(SUB_PATH)/net/socket/ssl_server_socket_impl.cc \
	$(SUB_PATH)/net/socket/tcp_client_socket.cc \
	$(SUB_PATH)/net/socket/ssl_connect_job.cc \
	$(SUB_PATH)/net/socket/tcp_server_socket.cc \
	$(SUB_PATH)/net/socket/tcp_socket_posix.cc \
	$(SUB_PATH)/net/socket/transport_client_socket.cc \
	$(SUB_PATH)/net/socket/transport_client_socket_pool.cc \
	$(SUB_PATH)/net/socket/transport_connect_job.cc \
	$(SUB_PATH)/net/socket/udp_client_socket.cc \
	$(SUB_PATH)/net/socket/udp_net_log_parameters.cc \
	$(SUB_PATH)/net/socket/udp_server_socket.cc \
	$(SUB_PATH)/net/socket/udp_socket_global_limits.cc \
	$(SUB_PATH)/net/socket/udp_socket_posix.cc \
	$(SUB_PATH)/net/socket/websocket_endpoint_lock_manager.cc \
	$(SUB_PATH)/net/socket/websocket_transport_client_socket_pool.cc \
	$(SUB_PATH)/net/ssl/cert_compression.cc \
	$(SUB_PATH)/net/ssl/client_cert_identity.cc \
	$(SUB_PATH)/net/ssl/openssl_ssl_util.cc \
	$(SUB_PATH)/net/ssl/ssl_cert_request_info.cc \
	$(SUB_PATH)/net/ssl/ssl_cipher_suite_names.cc \
	$(SUB_PATH)/net/ssl/ssl_client_auth_cache.cc \
	$(SUB_PATH)/net/ssl/ssl_client_session_cache.cc \
	$(SUB_PATH)/net/ssl/ssl_config.cc \
	$(SUB_PATH)/net/ssl/ssl_config_service.cc \
	$(SUB_PATH)/net/ssl/ssl_config_service_defaults.cc \
	$(SUB_PATH)/net/ssl/ssl_info.cc \
	$(SUB_PATH)/net/ssl/ssl_key_logger.cc \
	$(SUB_PATH)/net/ssl/ssl_platform_key_util.cc \
	$(SUB_PATH)/net/ssl/ssl_private_key.cc \
	$(SUB_PATH)/net/ssl/ssl_server_config.cc \
	$(SUB_PATH)/net/ssl/test_ssl_private_key.cc \
	$(SUB_PATH)/net/ssl/threaded_ssl_private_key.cc \
	$(SUB_PATH)/net/third_party/uri_template/uri_template.cc \
	$(SUB_PATH)/net/url_request/redirect_info.cc \
	$(SUB_PATH)/net/url_request/redirect_util.cc \
	$(SUB_PATH)/net/url_request/report_sender.cc \
	$(SUB_PATH)/net/url_request/static_http_user_agent_settings.cc \
	$(SUB_PATH)/net/url_request/url_fetcher.cc \
	$(SUB_PATH)/net/url_request/url_fetcher_core.cc \
	$(SUB_PATH)/net/url_request/url_fetcher_delegate.cc \
	$(SUB_PATH)/net/url_request/url_fetcher_impl.cc \
	$(SUB_PATH)/net/url_request/url_fetcher_response_writer.cc \
	$(SUB_PATH)/net/url_request/url_request.cc \
	$(SUB_PATH)/net/url_request/url_request_context.cc \
	$(SUB_PATH)/net/url_request/url_request_context_getter.cc \
	$(SUB_PATH)/net/url_request/url_request_context_storage.cc \
	$(SUB_PATH)/net/url_request/url_request_error_job.cc \
	$(SUB_PATH)/net/url_request/url_request_filter.cc \
	$(SUB_PATH)/net/url_request/url_request_http_job.cc \
	$(SUB_PATH)/net/url_request/url_request_interceptor.cc \
	$(SUB_PATH)/net/url_request/url_request_job.cc \
	$(SUB_PATH)/net/url_request/url_request_job_factory.cc \
	$(SUB_PATH)/net/url_request/url_request_netlog_params.cc \
	$(SUB_PATH)/net/url_request/url_request_redirect_job.cc \
	$(SUB_PATH)/net/url_request/url_request_test_job.cc \
	$(SUB_PATH)/net/url_request/url_request_throttler_entry.cc \
	$(SUB_PATH)/net/url_request/url_request_throttler_manager.cc \
	$(SUB_PATH)/net/url_request/websocket_handshake_userdata_key.cc \
	$(SUB_PATH)/third_party/ashmem/ashmem-dev.c \
	$(SUB_PATH)/third_party/modp_b64/modp_b64.cc \
	$(SUB_PATH)/url/gurl.cc \
	$(SUB_PATH)/url/origin.cc \
	$(SUB_PATH)/url/scheme_host_port.cc \
	$(SUB_PATH)/url/third_party/mozilla/url_parse.cc \
	$(SUB_PATH)/url/url_canon.cc \
	$(SUB_PATH)/url/url_canon_etc.cc \
	$(SUB_PATH)/url/url_canon_filesystemurl.cc \
	$(SUB_PATH)/url/url_canon_fileurl.cc \
	$(SUB_PATH)/url/url_canon_host.cc \
	$(SUB_PATH)/url/url_canon_icu.cc \
	$(SUB_PATH)/url/url_canon_internal.cc \
	$(SUB_PATH)/url/url_canon_ip.cc \
	$(SUB_PATH)/url/url_canon_mailtourl.cc \
	$(SUB_PATH)/url/url_canon_path.cc \
	$(SUB_PATH)/url/url_canon_pathurl.cc \
	$(SUB_PATH)/url/url_canon_query.cc \
	$(SUB_PATH)/url/url_canon_relative.cc \
	$(SUB_PATH)/url/url_canon_stdstring.cc \
	$(SUB_PATH)/url/url_canon_stdurl.cc \
	$(SUB_PATH)/url/url_constants.cc \
	$(SUB_PATH)/url/url_idna_icu.cc \
	$(SUB_PATH)/url/url_parse_file.cc \
	$(SUB_PATH)/url/url_util.cc \
	$(SUB_PATH)/base/rose/task_environment.cc \
	$(SUB_PATH)/net/server/http_server_rose.cc \
	$(SUB_PATH)/net/server/rdp_connection.cc \
	$(SUB_PATH)/net/server/rdp_server.cc \
	$(SUB_PATH)/net/server/rtsp_server.cc \
	$(SUB_PATH)/net/server/tcp_client_rose.cc \
	$(SUB_PATH)/net/server/tcp_connection.cc \
	$(SUB_PATH)/net/server/tcp_server.cc \
	$(SUB_PATH)/net/server/tcp_server_rose.cc \
	$(SUB_PATH)/net/url_request/url_request_http_job_rose.cc \
	$(SUB_PATH)/net/url_request/url_request_rose_util.cc \
	$(SUB_PATH)/net/ssl/ssl_config_service_bindcert.cpp