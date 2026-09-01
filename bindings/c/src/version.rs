// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

use crate::types::paimon_bytes;
use std::panic::{catch_unwind, AssertUnwindSafe};
use std::ptr;

/// ABI version for the native C boundary.
///
/// Version 1 is additive: callers must still feature-detect newer symbols when
/// loading the shared library dynamically.
#[no_mangle]
pub extern "C" fn paimon_abi_version() -> u32 {
    1
}

/// Return the paimon-rust package version as an owned UTF-8 byte buffer.
///
/// The returned bytes are not NUL terminated and must be released with
/// `paimon_bytes_free`.
#[no_mangle]
pub extern "C" fn paimon_library_version() -> paimon_bytes {
    catch_unwind(AssertUnwindSafe(|| {
        paimon_bytes::new(env!("CARGO_PKG_VERSION").as_bytes().to_vec())
    }))
    .unwrap_or(paimon_bytes {
        data: ptr::null_mut(),
        len: 0,
    })
}

const _: extern "C" fn() -> u32 = paimon_abi_version;
const _: extern "C" fn() -> paimon_bytes = paimon_library_version;
