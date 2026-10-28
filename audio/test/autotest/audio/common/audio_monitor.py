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
音频流监视器 — 基于基线对比法检测RENDER/CAPTURE流状态

用法:
    from audio.common.audio_monitor import AudioMonitor
    audio = AudioMonitor(driver, log_func=MESSAGE)

    # setup中采集基线
    audio.capture_baseline()

    # 操作后检测RENDER流(播放)
    has_render = audio.has_render_stream()

    # 轮询监测(瞬时事件如提示音)
    found = audio.monitor_render_stream(duration=3, interval=0.15)

    # 检测CAPTURE流(录音)
    has_capture = audio.has_capture_stream()
    found = audio.monitor_capture_stream(duration=3, interval=0.5)

依赖: shared/baseline_monitor.py
"""

import time
from shared.baseline_monitor import BaselineMonitor

# AudioPolicyService SA ID
AUDIO_POLICY_SA = "3009"

# 配置项关键字（需排除，避免假阳性）
CONFIG_EXCLUDE = [
    "maxRenderers", "maxFastRenderers", "maxCapturers", "maxFastCapturers", 
    "common config", "available", "size : 0",
]


class AudioMonitor:
    def __init__(self, driver, log_func=None):
        self.driver = driver
        self.log_func = log_func
        self.monitor = BaselineMonitor(driver, log_func)
        self.dump_cmd = f"hidumper -s {AUDIO_POLICY_SA}"

    @staticmethod
    def extract_render_lines(dump_text):
        """从dump中提取Renderer(播放流)相关行,排除配置项"""
        if not dump_text:
            return set()
        lines = set()
        for line in dump_text.split("\n"):
            line_stripped = line.strip()
            if not line_stripped:
                continue
            if any(skip in line_stripped for skip in CONFIG_EXCLUDE):
                continue
            if "Renderer" in line_stripped or "renderer" in line_stripped:
                lines.add(line_stripped)
            if any(k in line_stripped for k in ["RUNNING", "StreamOut", "OutputStream", "RENDER_STATE"]):
                lines.add(line_stripped)
        return lines

    @staticmethod
    def extract_capture_lines(dump_text):
        """从dump中提取Capturer(录音流)相关行,排除配置项"""
        if not dump_text:
            return set()
        lines = set()
        for line in dump_text.split("\n"):
            line_stripped = line.strip()
            if not line_stripped:
                continue
            if any(skip in line_stripped for skip in CONFIG_EXCLUDE):
                continue
            if "Capturer" in line_stripped or "capturer" in line_stripped:
                lines.add(line_stripped)
            if any(k in line_stripped for k in ["RUNNING", "StreamIn", "InputStream", "CAPTURE_STATE"]):
                lines.add(line_stripped)
        return lines

    def dump_audio_policy(self):
        """抓取AudioPolicyService(SA 3009)状态"""
        return self.monitor.dump_service(self.dump_cmd)

    def capture_baseline(self):
        """采集空闲态基线(RENDER + CAPTURE各采一份)"""
        self.monitor.capture_baseline(
            self.dump_cmd, extract_fn=self.extract_render_lines, key="render"
        )
        self.monitor.capture_baseline(
            self.dump_cmd, extract_fn=self.extract_capture_lines, key="capture"
        )

    def get_baseline_dump(self, key="render"):
        """获取基线dump文本"""
        dump_text, _ = self.monitor.get_baseline(key)
        return dump_text

    def has_render_stream(self, dump_text=None):
        """检测是否存在RENDER(播放)流 — 基线对比法"""
        if dump_text is None:
            dump_text = self.dump_audio_policy()
        return self.monitor.has_new_lines_from_dump(
            dump_text, self.extract_render_lines, key="render"
        )

    def has_capture_stream(self, dump_text=None):
        """检测是否存在CAPTURE(录音)流 — 基线对比法"""
        if dump_text is None:
            dump_text = self.dump_audio_policy()
        return self.monitor.has_new_lines_from_dump(
            dump_text, self.extract_capture_lines, key="capture"
        )

    def monitor_render_stream(self, duration=5, interval=0.3, break_on_found=False):
        """轮询监测RENDER流(用于捕获瞬时提示音)

        参数：
            duration: 监测时长(秒)
            interval: 轮询间隔(秒),瞬时事件建议0.15
            break_on_found: True=找到即退出

        返回: True=检测到RENDER流出现过
        """
        return self.monitor.monitor_stream(
            self.dump_cmd, self.extract_render_lines,
            duration=duration, interval=interval,
            key="render", break_on_found=break_on_found
        )

    def monitor_capture_stream(self, duration=3, interval=0.5, break_on_found=True):
        """轮询监测CAPTURE流(用于验证录音持续中)

        参数：
            duration: 监测时长(秒)
            interval: 轮询间隔(秒)
            break_on_found: True=找到即退出(默认，录音流一旦出现即可确认)

        返回: True=检测到CAPTURE流
        """
        return self.monitor.monitor_stream(
            self.dump_cmd, self.extract_capture_lines,
            duration=duration, interval=interval,
            key="capture", break_on_found=break_on_found
        )

    def has_render_stream_vs(self, dump_text, baseline_dump):
        """对比指定dump与指定基线(非预采集基线,用于录像中->停止对比)"""
        current = self.extract_render_lines(dump_text)
        baseline = self.extract_render_lines(baseline_dump) if baseline_dump else set()
        new_lines = current - baseline
        if new_lines:
            self._log(f"[RENDER对比] 新增行: {new_lines}")
        return len(new_lines) > 0

    def has_capture_stream_vs(self, dump_text, baseline_dump):
        """对比指定dump与指定基线(非预采集基线)"""
        current = self.extract_capture_lines(dump_text)
        baseline = self.extract_capture_lines(baseline_dump) if baseline_dump else set()
        new_lines = current - baseline
        if new_lines:
            self._log(f"[CAPTURE对比] 新增行: {new_lines}")
        return len(new_lines) > 0

    def _log(self, msg):
        if self.log_func:
            self.log_func(msg)
