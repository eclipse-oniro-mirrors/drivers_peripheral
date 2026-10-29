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
录像文件辅助工具 — 查找设备端最新录像文件 + 音频轨检测

用法:
    from audio.common.video_helper import VideoHelper
    vh = VideoHelper(driver, log_func=MESSAGE)

    # 查找最新录像文件
    path = vh.get_latest_video_path()

    # 检查是否含音频轨
    has_audio = vh.check_video_has_audio_track(path)
"""


class VideoHelper:
    def __init__(self, driver, log_func=None):
        self.driver = driver
        self.log_func = log_func

    def get_latest_video_path(self):
        """查询设备上最新录像文件路径

        4级回退策略:
            1.Video目录下VID_*.mp4按时间倒序
            2.Photo目录下VID_*.mp4按时间倒序
            3.全盘查找最近5分钟的VID_*.mp4
            4.全盘查找最新5分钟的任意mp4

        全部排除.thumbs缩略图目录
        """
        strategies = [
            "ls -t /storage/media/100/local/files/Video/VID_*.mp4 2>/dev/null | head -1",
            "ls -t /storage/media/100/local/files/Photo/VID_*.mp4 2>/dev/null | head -1",
            "find /storage/media/100/local/files -name 'VID_*.mp4' -mmin -5 2>/dev/null | grep -v '.thumbs' | head -1",
            "find /storage/media/100/local/files -name '*.mp4' -mmin -5 2>/dev/null | grep -v '.thumbs' | head -1",
        ]

        for cmd in strategies:
            out = self.driver.shell(cmd)
            path = out.strip().strip("\n") if out else ""
            if path and ".thumbs" not in path:
                self._log(f"[录像文件] 找到: {path}")
                return path

        self._log("[录像文件] 未找到录像文件")
        return None

    def get_latest_photo_path(self):
        """查询设备上最新照片文件路径

        4级回退策略,排除.thumbs
        """
        strategies = [
            "ls -t /storage/media/100/local/files/Photo/IMG_*.jpg 2>/dev/null | head -1",
            "ls -t /storage/media/100/local/files/Photo/IMG_*.jpeg 2>/dev/null | head -1",
            "find /storage/media/100/local/files -name 'IMG_*.jpg' -mmin -5 2>/dev/null | grep -v '.thumbs' | head -1",
            "find /storage/media/100/local/files -name '*.jpg' -mmin -5 2>/dev/null | grep -v '.thumbs' | head -1",
        ]

        for cmd in strategies:
            out = self.driver.shell(cmd)
            path = out.strip().strip("\n") if out else ""
            if path and ".thumbs" not in path:
                self._log(f"[照片文件] 找到: {path}")
                return path

        self._log("[照片文件] 未找到照片文件")
        return None

    def check_video_has_audio_track(self, device_video_path):
        """检查录像文件是否包含音频轨

        MP4中音频handler type: 'soun' (sound), 编码: 'mp4a' (AAC)
        用strings + grep全文搜索(不限于文件头)
        """
        if not device_video_path:
            self._log("[音频轨] 设备上未找到录像文件")
            return False

        ret = self.driver.shell(f"ls -la {device_video_path}")
        self._log(f"[音频轨] 设备端录像文件信息: {ret}")

        try:
            # 复制到tmp目录便于操作
            self.driver.shell(f"cp {device_video_path} /data/local/tmp/recorded_video.mp4")

            # strings全文搜索音频标识
            out = self.driver.shell(
                "strings /data/local/tmp/recorded_video.mp4 | "
                "grep -iE 'soun|mp4a|aac|opus|audio|ac-3|ec-3' | head -20"
            )
            self._log(f"[音频轨] 搜索结果: {out}")

            if out and any(k in out.lower() for k in ["soun", "mp4a", "aac", "opus", "audio"]):
                self._log("[音频轨] 录像文件包含音频轨")
                return True
 
            # 备用: grep -a 直接在二进制中搜索
            out2 = self.driver.shell("grep -ac 'soun' /data/local/tmp/recorded_video.mp4 2>/dev/null || echo 0")
            self._log(f"[音频轨] grep soun 计数: {out2}")
            if out2 and out2.strip() != "0":
                self._log("[音频轨] 录像文件包含音频轨(grep -a soun 命中)")
                return True

            self._log("[音频轨] 录像文件未检测到音频轨")
            return False
        except Exception as e:
            self._log(f"[音频轨] 检查异常: {e}")
            return False

    def count_photos_in_dir(self, dir_path="/storage/media/100/local/files/Photo/"):
        """统计指定目录下的照片数量"""
        out = self.driver.shell(f"ls {dir_path}/IMG_*.jpg 2>/dev/null | wc -l")
        try:
            count = int(out.strip())
            self._log(f"[照片统计] {dir_path}: {count}张照片")
            return count
        except (ValueError, AttributeError):
            self._log(f"[照片统计] 无法解析: {out}")
            return 0

    def count_videos_in_dir(self, dir_path="/storage/media/100/local/files/Photo/"):
        """统计指定目录下的录像数量"""
        out = self.driver.shell(f"ls {dir_path}/VID_*.mp4 2>/dev/null | wc -l")
        try:
            count = int(out.strip())
            self._log(f"[录像统计] {dir_path}: {count}个视频")
            return count
        except (ValueError, AttributeError):
            self._log(f"[录像统计] 无法解析: {out}")
            return 0

    def check_file_size_nonzero(self, device_path):
        """检查文件存在且大小>0"""
        out = self.driver.shell(f"ls -la {device_path} 2>/dev/null")
        if not out or "No such file" in out:
            return False
        # 解析文件大小(ls -la第5列)
        parts = out.strip().split()
        if len(parts) >= 5:
            try:
                size = int(parts[4])
                self._log(f"[文件校验] {device_path} 大小={size}字节")
                return size > 0
            except ValueError:
                pass
        return False

    def _log(self, msg):
        if self.log_func:
            self.log_func(msg)
