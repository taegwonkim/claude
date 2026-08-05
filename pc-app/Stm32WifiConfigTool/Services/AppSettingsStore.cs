using System;
using System.Collections.Generic;
using System.IO;
using Stm32WifiConfigTool.Models;

namespace Stm32WifiConfigTool.Services
{
    /// <summary>
    /// AppSettings를 "%AppData%\Stm32WifiConfigTool\settings.ini"에 간단한 key=value 텍스트로
    /// 저장/로드한다. 외부 패키지나 System.Configuration/XML 직렬화 의존 없이 최소 구현.
    /// 파일이 없거나 손상된 경우 기본값(new AppSettings())으로 안전하게 대체한다.
    /// </summary>
    public static class AppSettingsStore
    {
        private static readonly string FilePath = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "Stm32WifiConfigTool", "settings.ini");

        public static AppSettings Load()
        {
            var settings = new AppSettings();

            if (!File.Exists(FilePath))
            {
                return settings;
            }

            try
            {
                Dictionary<string, string> map = ReadKeyValues(FilePath);

                ApplyChannel(map, "Usb", settings.Usb);
                ApplyChannel(map, "Uart", settings.Uart);

                settings.WifiCommandChannel = GetString(map, "WifiCommandChannel", settings.WifiCommandChannel);
                settings.WifiCommandTimeoutMs = GetInt(map, "WifiCommandTimeoutMs", settings.WifiCommandTimeoutMs);
                settings.MeasurementDisplayChannel = GetString(map, "MeasurementDisplayChannel", settings.MeasurementDisplayChannel);
                settings.MeasurementAutoScroll = GetBool(map, "MeasurementAutoScroll", settings.MeasurementAutoScroll);
                settings.EspStatusDisplayChannel = GetString(map, "EspStatusDisplayChannel", settings.EspStatusDisplayChannel);

                return settings;
            }
            catch (Exception)
            {
                /* 설정 파일 손상/파싱 실패 - 기본값으로 안전하게 폴백 */
                return new AppSettings();
            }
        }

        public static void Save(AppSettings settings)
        {
            string dir = Path.GetDirectoryName(FilePath);
            if (!string.IsNullOrEmpty(dir) && !Directory.Exists(dir))
            {
                Directory.CreateDirectory(dir);
            }

            var lines = new List<string>
            {
                "# Stm32WifiConfigTool 설정 파일 - 프로그램이 종료 시 자동 저장합니다.",
                "Usb.PortName=" + settings.Usb.PortName,
                "Usb.BaudRate=" + settings.Usb.BaudRate,
                "Usb.ReadTimeoutMs=" + settings.Usb.ReadTimeoutMs,
                "Usb.WriteTimeoutMs=" + settings.Usb.WriteTimeoutMs,
                "Uart.PortName=" + settings.Uart.PortName,
                "Uart.BaudRate=" + settings.Uart.BaudRate,
                "Uart.ReadTimeoutMs=" + settings.Uart.ReadTimeoutMs,
                "Uart.WriteTimeoutMs=" + settings.Uart.WriteTimeoutMs,
                "WifiCommandChannel=" + settings.WifiCommandChannel,
                "WifiCommandTimeoutMs=" + settings.WifiCommandTimeoutMs,
                "MeasurementDisplayChannel=" + settings.MeasurementDisplayChannel,
                "MeasurementAutoScroll=" + settings.MeasurementAutoScroll,
                "EspStatusDisplayChannel=" + settings.EspStatusDisplayChannel
            };

            File.WriteAllLines(FilePath, lines);
        }

        private static void ApplyChannel(Dictionary<string, string> map, string prefix, ChannelSettings channel)
        {
            channel.PortName = GetString(map, prefix + ".PortName", channel.PortName);
            channel.BaudRate = GetInt(map, prefix + ".BaudRate", channel.BaudRate);
            channel.ReadTimeoutMs = GetInt(map, prefix + ".ReadTimeoutMs", channel.ReadTimeoutMs);
            channel.WriteTimeoutMs = GetInt(map, prefix + ".WriteTimeoutMs", channel.WriteTimeoutMs);
        }

        private static Dictionary<string, string> ReadKeyValues(string path)
        {
            var map = new Dictionary<string, string>();
            foreach (string rawLine in File.ReadAllLines(path))
            {
                string line = rawLine.Trim();
                if (line.Length == 0 || line.StartsWith("#", StringComparison.Ordinal))
                {
                    continue;
                }
                int eq = line.IndexOf('=');
                if (eq <= 0)
                {
                    continue;
                }
                string key = line.Substring(0, eq).Trim();
                string value = line.Substring(eq + 1).Trim();
                map[key] = value;
            }
            return map;
        }

        private static string GetString(Dictionary<string, string> map, string key, string fallback)
        {
            return map.TryGetValue(key, out string value) ? value : fallback;
        }

        private static int GetInt(Dictionary<string, string> map, string key, int fallback)
        {
            if (map.TryGetValue(key, out string value) && int.TryParse(value, out int parsed))
            {
                return parsed;
            }
            return fallback;
        }

        private static bool GetBool(Dictionary<string, string> map, string key, bool fallback)
        {
            if (map.TryGetValue(key, out string value) && bool.TryParse(value, out bool parsed))
            {
                return parsed;
            }
            return fallback;
        }
    }
}
