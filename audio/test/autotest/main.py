# !/usr/bin/env python
# -*- coding: utf-8 -*-

#
# Copyright (c) 2026 Huawei Device Co., Ltd.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""
audio_all — audio模块统一执行入口

用法：
    python main.py

说明：
    按编号顺序执行testcases/下所有audio用例,
    某条用例失败不影响后续用例执行,
    最终生成汇总报告。
"""

from xdevice.__main__ import main_process

if __name__ == "__main__":
    # 按编号顺序列出所有audio用例(分号分割)
    # xdevice对每条用例独立执行,失败不中断后续
    test_list = ";".join([
        "audio_0001_camera_record_playback",
        "audio_0010_reboot_verification",
    ])
    main_process(f"run -l {test_list} -ta agent_mode:bin;screenshot:true")
