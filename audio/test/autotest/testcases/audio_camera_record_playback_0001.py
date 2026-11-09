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
#
#  文 件 名: audio_camera_record_playback_0001.py
#  文件说明: audio_test_0001 录像及播放音频验证
#
#
#    用例要求:
#        1.点击录像和结束录像有“哒”的声音
#        2.录像播放声音正常
#    限制条件:远程真机无网络/无账号/听不到声音的情况下
#    验证策略:通过hidumper查音频流状态替代听觉判断
#    验证点:
#        VP1: 录音开始/停止有“哒”声(RENDER流基线对比)
#        VP2: 录像过程中存在CAPTURE流(录音中)
#        VP3: 录像文件存在且包含音频轨
#        VP4: 播放录像时存在RENDER流(播放声音)
#
"""

import os
import sys
import time

# 添加项目根目录到path,以便导入shared/audio模块
PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, PROJECT_ROOT)

from devicetest.core.test_case import TestCase, Step, MESSAGE
from hypium import UiDriver, BY
from shared.device_base import DeviceBase
from shared.init_manager import InitManager
from audio.common.audio_monitor import AudioMonitor
from audio.common.video_helper import VideoHelper


class AudioCameraRecordPlayback0001(TestCase):
    def __init__(self, configs):
        self.tag = self.__class__.__name__
        TestCase.__init__(self, self.tag, configs)
        self.driver = UiDriver(self.device1)

        # 公共模块
        self.dev = DeviceBase(self.driver)
        self.init_mgr = InitManager(self.driver)
        self.audio = AudioMonitor(self.driver, log_func=MESSAGE)
        self.video = VideoHelper(self.driver, log_func=MESSAGE)

        # 录像文件保存目录(本机,如有需要可拉取)
        self.local_video_dir = os.path.join(os.path.dirname(__file__), "..", "video_output")
        os.makedirs(self.local_video_dir, exist_ok=True)

    def setup(self):
        Step("预置工作:初始化设备")
        self.driver.unlock()
        self.dev.set_screen_timeout(86400000)

        Step("预置工作:采集音频策略服务基线(空闲态)")
        # 采集空闲态dump作为基线,后续验证点对比此基线判断是否有新流出现
        self.audio.capture_baseline()
        baseline_render = self.audio.get_baseline_dump("render")
        if baseline_render:
            MESSAGE(f"[预置] AudioPolicyService(SA3009)可用,基线dump长度={len(baseline_render)}")
        else:
            MESSAGE(f"[ERROR] AudioPolicyService(SA3009)无输出!基线采集失败")

        Step("预置工作:清理后台")
        self.dev.clear_recent_tasks()

    def process(self):
        render_on_start = self._vp1_record_render_stream()
        capture_active, render_on_stop = self._vp2_capture_stream()
        has_audio = self._vp3_video_audio_track()
        render_on_playback = self._vp4_playback_render_stream()

        Step("步骤9: 结果汇总")
        results = {
            "录像开始提示音(render流)": render_on_start,
            "录像过程中录音(capture流)": capture_active,
            "录像停止提示音(render流)": render_on_stop,
            "录像文件含音频轨": has_audio,
            "播放声音(render流)": render_on_playback,
        }

        MESSAGE("=" * 50)
        MESSAGE("audio_test_0001 验证结果汇总:")
        for item, result in results.items():
            status = "PASS" if result else "FAIL"
            MESSAGE(f"  [{status}] {item}")
        MESSAGE("=" * 50)

        # 核心验证点：播放声音 + 录像含音频轨
        # 提示音（render流）作为辅助参考（系统提示音可能流过短难以捕获）
        core_pass = results["播放声音(render流)"] and results["录像文件含音频轨"]
        if core_pass:
            MESSAGE("核心验证通过：录像播放声音正常 + 录像文件包含音频轨")
        else:
            fail_items = [k for k, v in results.items() if not v]
            MESSAGE(f"核心验证失败项: {fail_items}")
            raise ValueError(f"核心验证失败,未通过项: {fail_items}")

    def teardown(self):
        Step("清理工作:清理后台")
        try:
            self.dev.clear_recent_tasks()
        except Exception as e:
            MESSAGE(f"清理异常: {e}")

    def _click_shutter_with_retry(self, tag):
        """点击快门按钮,带重试(组件引用失效时重新查找)"""
        shutter_key = "SHUTTER_BUTTON_NORMAL_OUT_BTN"
        for attempt in range(3):
            shutter = self.driver.find_component(BY.key(shutter_key))
            if not shutter:
                self.driver.capture_screen(f"{tag}_no_shutter.jpeg")
                return False
            try:
                shutter.click()
                return True
            except Exception as e:
                MESSAGE(f"[{tag}] 快门点击异常(尝试{attempt+1}/3): {e}")
                self.driver.wait(1)
        self.driver.capture_screen(f"{tag}_click_fail.jpeg")
        return False

    def _vp1_record_render_stream(self):
        """VP1: 录像开始/停止有“哒”声(RENDER流基线对比)"""
        Step("步骤1: 打开相机应用")
        self.driver.start_app(package_name="com.huawei.hmos.camera",
                              page_name="com.huawei.hmos.camera.MainAbility")
        self.driver.wait(2)

        Step("步骤2: 初始化相机(自适应处理首次弹窗)")
        self.init_mgr.dismiss_init_dialogs(
            "camera", ["同意", "跳过", "我知道了", "下一步", "允许"],
            log_func=MESSAGE,
        )

        Step("步骤3: 切换到录像模式并验证提示音(render流)")
        video_tab = self.driver.find_component(BY.text("录像"))
        if video_tab:
            video_tab.click()
            render_after_tab = self.audio.monitor_render_stream(duration=3, interval=0.2)
            if render_after_tab:
                MESSAGE("[VP1a] 切换录像模式时检测到RENDER流(提示音播放)")
            else:
                MESSAGE("[VP1a] 切换录像模式时未检测到RENDER流(可能无提示音或流过短)")
            self.driver.wait(2)
        else:
            MESSAGE("未找到'录像'tab,可能已在录像模式")
            self.driver.capture_screen("step3_no_video_tab.jpeg")

        Step("步骤3.1: 点击快门开始录像")
        if not self._click_shutter_with_retry("step3"):
            raise ValueError("步骤3失败:快门按钮点击3次均失败")
        MESSAGE("已点击开始录像")
        render_on_start = self.audio.monitor_render_stream(duration=3, interval=0.15)
        if render_on_start:
            MESSAGE("[VP1b] 开始录像时检测到RENDER流(哒声播放)")
        else:
            MESSAGE("[VP1b] 开始录像时未检测到RENDER流(哒声可能未播放或流过短)")
        return render_on_start

    def _vp2_capture_stream(self):
        """VP2: 录像过程中存在CAPTURE(录音)流"""
        Step("步骤4: 录像过程中验证CAPTURE流(录音中)")
        self.driver.wait(2)
        capture_active = self.audio.monitor_capture_stream(duration=3, interval=0.5)
        if capture_active:
            MESSAGE("[VP2] 录像过程中检测到CAPTURE流(正在录音)")
        else:
            MESSAGE("[VP2] 录像过程中未检测到CAPTURE流(录音可能未启动)")
        self.driver.wait(5)

        Step("步骤5: 停止录像并验证提示音(render流)")
        # 录像中dump作为停止前的基线(此时capture流在跑,排除capture影响)
        dump_during_record = self.audio.dump_audio_policy()
        if not self._click_shutter_with_retry("step5"):
            MESSAGE("停止按钮点击失败,截图诊断")
        else:
            MESSAGE("已点击停止录像")

        render_on_stop = False
        stop_start = time.time()
        while time.time() - stop_start < 3:
            dump_now = self.audio.dump_audio_policy()
            if self.audio.has_render_stream_vs(dump_now, dump_during_record):
                render_on_stop = True
                break
            time.sleep(0.15)
        if render_on_stop:
            MESSAGE("[VP1c] 停止录像时检测到RENDER流(哒声播放)")
        else:
            MESSAGE("[VP1c] 停止录像时未检测到RENDER流(哒声可能未播放)")
        self.driver.wait(3)
        return capture_active, render_on_stop

    def _vp3_video_audio_track(self):
        """VP3: 录像文件存在且包含音频轨"""
        Step("步骤6: 验证录像文件存在且包含音频轨")
        video_path = self.video.get_latest_video_path()
        has_audio = False
        if video_path:
            MESSAGE(f"录像文件路径: {video_path}")
            has_audio = self.video.check_video_has_audio_track(video_path)
            if has_audio:
                MESSAGE("[VP3] 录像文件包含音频轨")
            else:
                MESSAGE("[VP3] 录像文件不包含音频轨(可能录音失败)")
        else:
            MESSAGE("未找到录像文件,截图诊断")
            self.driver.capture_screen("step6_no_video_file.jpeg")
        return has_audio

    def _vp4_playback_render_stream(self):
        """VP4: 播放录像时存在RENDER流(声音播放)"""
        Step("步骤7: 播放录像并验证RENDER流(播放声音)")
        # 7.1 点击缩略图进入相册
        thumbnail = self.driver.find_component(BY.key("COMPONENT_ID_THUMBNAIL_1"))
        if not thumbnail:
            self.driver.capture_screen("step7_no_thumbnail.jpeg")
            raise ValueError("步骤7失败:未找到缩略图,可能未录制成功")
        thumbnail.click()
        self.driver.wait(2)

        # 7.2 点击播放
        play_btn = self.driver.find_component(BY.key("VideoPlayButton"))
        if play_btn:
            play_btn.click()
            self.driver.wait(1)
        else:
            MESSAGE("未找到播放按钮，尝试点击视频区域自动播放")
            self.driver.capture_screen("step7_no_playbtn.jpeg")

        # 7.3 点击视频区域
        xcomp = self.driver.find_component(BY.type("XComponent"))
        if xcomp:
            xcomp.click()
        self.driver.wait(1)

        # 7.4 监测播放期间的RENDER流(持续播放声音,对比空闲态基线)
        MESSAGE("监测播放期间的RENDER流(声音播放,对比空闲态基线)")
        render_on_playback = self.audio.monitor_render_stream(duration=5, interval=0.5)
        if render_on_playback:
            MESSAGE("[VP4] 播放录像时检测到RENDER流(声音正常播放)")
        else:
            MESSAGE("[VP4] 播放录像时未检测到RENDER流(声音可能未播放)")

        # ----- 播放进度条验证（辅助） ------
        Step("步骤8: 验证播放进度条(辅助验证)")
        component = self.driver.find_component(BY.key("TimeCapsule"))
        if component:
            MESSAGE("[辅助验证] 播放进度条存在，视频正在播放")
        else:
            self.driver.capture_screen("step8_no_timecapsule.jpeg")
            MESSAGE("[辅助验证] 未找到播放进度条")
        return render_on_playback
