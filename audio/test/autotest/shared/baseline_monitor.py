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
基线对比监测器 — 通用基线对比法检测实时流/状态变化

用法:
    from shared.baseline_monitor import BaselineMonitor
    monitor = BaselineMonitor(driver)

    # 采集基线
    monitor.capture_baseline("hidumper -s 3009", extract_fn=monitor.extract_render_lines)

    # 操作后对比
    has_new = monitor.has_new_lines("hidumper -s 3009", extract_fn=monitor.extract_render_lines)
    # has_new=True 说明有新增行(实时流出现)

原理:
    - dump输出混合配置项(maxRenderers:256)和实时实例(stream size:1)
    - 关键字匹配法无法区分,配置项导致恒PASS假阳性
    - 基线对比法:记录空闲态dump,操作后对比新增行,只看“有没有变化”
"""

import time


class BaselineMonitor:
    def __init__(self, driver, log_func=None):
        self.driver = driver
        self.log_func = log_func
        self.baselines = {}  # key -> (dump_text, lines_set)

    @staticmethod
    def exclude_config_lines(dump_text, exclude_keywords):
        """通用提取函数:从dump中提取行,排除含指定关键字的配置项
        
        参数:
            dump_text: dump文本
            exclude_keywords: 需排除的关键字列表(如["maxRenderers", "common config"])

        返回: 行集合(set)
        """
        if not dump_text:
            return set()
        lines = set()
        for line in dump_text.split("\n"):
            line_stripped = line.strip()
            if not line_stripped:
                continue
            if any(skip in line_stripped for skip in exclude_keywords):
                continue
            lines.add(line_stripped)
        return lines

    @staticmethod
    def filter_lines(dump_text, include_keywords, exclude_keywords=None):
        """通用提取函数:从dump中提取含指定关键字的行,排除配置项

        参数:
            dump_text: dump文本
            include_keywords: 需包含的关键字列表(满足任一即可)
            exclude_keywords: 需排除的关键字列表
        
        返回: 行集合(set)
        """
        if not dump_text:
            return set()
        exclude_keywords = exclude_keywords or []
        lines = set()
        for line in dump_text.split("\n"):
            line_stripped = line.strip()
            if not line_stripped:
                continue
            if any(skip in line_stripped for skip in exclude_keywords):
                continue
            if any(k in line_stripped for k in include_keywords):
                lines.add(line_stripped)
        return lines

    def dump_service(self, cmd):
        """执行dump命令并返回输出文本"""
        out = self.driver.shell(cmd)
        return out if out else ""

    def capture_baseline(self, dump_cmd, extract_fn=None, key="default"):
        """采集基线dump
        
        参数:
            dump_cmd: dump命令(如"hidumper -s 3009")
            extract_fn: 提取函数,从dump文本中提取相关集合(如extract_render_lines)
            key: 基线标识,支持多组基线(如"render"/"capture")
        """
        dump_text = self.dump_service(dump_cmd)
        lines = set()
        if extract_fn and dump_text:
            lines = set(extract_fn(dump_text))
        self.baselines[key] = (dump_text, lines)
        self._log(f"[基线] {key} 采集完成: dump长度={len(dump_text)}, 提取行数={len(lines)}")
        return dump_text

    def get_baseline(self, key="default"):
        """获取已采集的基线(dump_text, lines_set)"""
        return self.baselines.get(key, ("", set()))

    def has_new_lines(self, dump_cmd, extract_fn, key="default"):
        """对比当前dump与基线,判断是否有新增行
        
        参数:
            dump_cmd: dump命令
            extract_fn: 提取函数
            key: 基线标识

        返回: True=有新增行(检测到实时流/状态变化)
        """

        current_dump = self.dump_service(dump_cmd)
        current_lines = set(extract_fn(current_dump)) if current_dump else set()
        _, baseline_lines = self.get_baseline(key)
        new_lines = current_lines - baseline_lines
        if new_lines:
            self._log(f"[基线对比] {key} 新增行: {new_lines}")
        return len(new_lines) > 0

    def has_new_lines_from_dump(self, dump_text, extract_fn, key="default"):
        """从已有dump文本对比基线(避免重复dump)"""
        current_lines = set(extract_fn(dump_text)) if dump_text else set()
        _, baseline_lines = self.get_baseline(key)
        new_lines = current_lines - baseline_lines
        if new_lines:
            self._log(f"[基线对比] {key} 新增行: {new_lines}")
        return len(new_lines) > 0

    def monitor_stream(self, dump_cmd, extract_fn, duration=5, interval=0.3,
                       key="default", break_on_found=False):
        """在指定时长内轮询监测是否出现新增行(实时流)

        参数:
            dump_cmd: dump命令
            extract_fn: 提取函数
            duration: 监测时长(秒)
            interval: 轮询间隔(秒)
            key: 基线标识
            break_on_found: True=找到即退出(瞬时事件), False=持续监测(持续事件)

        返回: True=检测到新增行出现过
        """
        start = time.time()
        found = False
        samples = 0
        hits = 0
        while time.time() - start < duration:
            has_new = self.has_new_lines(dump_cmd, extract_fn, key)
            samples += 1
            if has_new:
                found = True
                hits += 1
                if break_on_found:
                    break
            time.sleep(interval)

        self._log(f"[监测] {key}: duration={duration}s, samples={samples}, hits={hits}, found={found}")
        return found

    def _log(self, msg):
        if self.log_func:
            self.log_func(msg)
