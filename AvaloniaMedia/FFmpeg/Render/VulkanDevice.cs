using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Silk.NET.Vulkan;
using VkQueue = Silk.NET.Vulkan.Queue;

namespace AvaloniaMedia.FFmpeg.Render;

public unsafe class VulkanDevice : IDisposable
{
    private Vk _vk;
    private Instance _instance;
    private PhysicalDevice _physicalDevice;
    private Device _device;
    private VkQueue _graphicsQueue;
    private VkQueue _computeQueue;
    private uint _graphicsQueueFamilyIndex;
    private uint _computeQueueFamilyIndex;
    private bool _disposed;

    public Instance VkInstance => _instance;
    public PhysicalDevice VkPhysicalDevice => _physicalDevice;
    public Device VkDevice => _device;
    public VkQueue GraphicsQueue => _graphicsQueue;
    public VkQueue ComputeQueue => _computeQueue;
    public uint GraphicsQueueFamilyIndex => _graphicsQueueFamilyIndex;
    public uint ComputeQueueFamilyIndex => _computeQueueFamilyIndex;
    public Vk VkApi => _vk;

    public bool IsValid => _device.Handle != 0;

    public VulkanDevice()
    {
        _vk = Vk.GetApi();

        var appInfo = new ApplicationInfo
        {
            SType = StructureType.ApplicationInfo,
            PApplicationName = (byte*)Marshal.StringToHGlobalAnsi("AvaloniaMedia"),
            ApplicationVersion = Vk.Version10,
            PEngineName = (byte*)Marshal.StringToHGlobalAnsi("AvaloniaMedia"),
            EngineVersion = Vk.Version10,
            ApiVersion = Vk.Version13
        };

        // --- Instance ---
        var enabledExtensions = new List<string>();
        var availableExtNames = GetAvailableInstanceExtensions();

        if (availableExtNames.Contains("VK_KHR_surface"))
            enabledExtensions.Add("VK_KHR_surface");
        if (availableExtNames.Contains("VK_KHR_win32_surface"))
            enabledExtensions.Add("VK_KHR_win32_surface");
        if (availableExtNames.Contains("VK_KHR_portability_enumeration"))
            enabledExtensions.Add("VK_KHR_portability_enumeration");

        var extPtrs = new IntPtr[enabledExtensions.Count];
        for (int i = 0; i < enabledExtensions.Count; i++)
            extPtrs[i] = Marshal.StringToHGlobalAnsi(enabledExtensions[i]);

        var instanceCreateInfo = new InstanceCreateInfo
        {
            SType = StructureType.InstanceCreateInfo,
            PApplicationInfo = &appInfo,
            EnabledExtensionCount = (uint)enabledExtensions.Count
        };

        fixed (IntPtr* pExt = extPtrs)
        {
            instanceCreateInfo.PpEnabledExtensionNames = (byte**)pExt;
            if (availableExtNames.Contains("VK_KHR_portability_enumeration"))
                instanceCreateInfo.Flags |= InstanceCreateFlags.EnumeratePortabilityBitKhr;

            fixed (Instance* pInstance = &_instance)
                _vk.CreateInstance(&instanceCreateInfo, null, pInstance).Check();
        }

        Console.WriteLine("[Vulkan] Instance created.");

        // --- Physical Device ---
        uint deviceCount = 0;
        _vk.EnumeratePhysicalDevices(_instance, &deviceCount, null);
        var physicalDevices = new PhysicalDevice[deviceCount];
        fixed (PhysicalDevice* pDevices = physicalDevices)
            _vk.EnumeratePhysicalDevices(_instance, &deviceCount, pDevices);

        _physicalDevice = PickPhysicalDevice(physicalDevices);
        if (_physicalDevice.Handle == 0)
            throw new InvalidOperationException("No suitable Vulkan physical device found.");

        _vk.GetPhysicalDeviceProperties(_physicalDevice, out var deviceProps);
        string deviceName = GetFixedString(deviceProps.DeviceName);
        Console.WriteLine($"[Vulkan] Physical device: {deviceName}");

        // --- Queue Families ---
        uint queueFamilyCount = 0;
        _vk.GetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueFamilyCount, null);
        var queueFamilyProps = new QueueFamilyProperties[queueFamilyCount];
        fixed (QueueFamilyProperties* pProps = queueFamilyProps)
            _vk.GetPhysicalDeviceQueueFamilyProperties(_physicalDevice, &queueFamilyCount, pProps);

        _graphicsQueueFamilyIndex = uint.MaxValue;
        _computeQueueFamilyIndex = uint.MaxValue;

        for (uint i = 0; i < queueFamilyCount; i++)
        {
            if ((queueFamilyProps[i].QueueFlags & QueueFlags.GraphicsBit) != 0
                && (queueFamilyProps[i].QueueFlags & QueueFlags.ComputeBit) != 0)
            {
                _graphicsQueueFamilyIndex = i;
                _computeQueueFamilyIndex = i;
                break;
            }
        }

        if (_graphicsQueueFamilyIndex == uint.MaxValue)
        {
            for (uint i = 0; i < queueFamilyCount; i++)
            {
                if ((queueFamilyProps[i].QueueFlags & QueueFlags.GraphicsBit) != 0
                    && _graphicsQueueFamilyIndex == uint.MaxValue)
                    _graphicsQueueFamilyIndex = i;
                if ((queueFamilyProps[i].QueueFlags & QueueFlags.ComputeBit) != 0
                    && _computeQueueFamilyIndex == uint.MaxValue)
                    _computeQueueFamilyIndex = i;
            }
        }

        if (_graphicsQueueFamilyIndex == uint.MaxValue)
            throw new InvalidOperationException("No graphics queue family found.");

        // --- Logical Device ---
        float queuePriority = 1.0f;
        var queueCreateInfos = new List<DeviceQueueCreateInfo>
        {
            new DeviceQueueCreateInfo
            {
                SType = StructureType.DeviceQueueCreateInfo,
                QueueFamilyIndex = _graphicsQueueFamilyIndex,
                QueueCount = 1,
                PQueuePriorities = &queuePriority
            }
        };

        if (_computeQueueFamilyIndex != _graphicsQueueFamilyIndex)
        {
            queueCreateInfos.Add(new DeviceQueueCreateInfo
            {
                SType = StructureType.DeviceQueueCreateInfo,
                QueueFamilyIndex = _computeQueueFamilyIndex,
                QueueCount = 1,
                PQueuePriorities = &queuePriority
            });
        }

        var deviceExtensions = new List<string>();
        var availableDeviceExtNames = GetAvailableDeviceExtensions();

        if (availableDeviceExtNames.Contains("VK_KHR_swapchain"))
            deviceExtensions.Add("VK_KHR_swapchain");

        var deviceExtPtrs = new IntPtr[deviceExtensions.Count];
        for (int i = 0; i < deviceExtensions.Count; i++)
            deviceExtPtrs[i] = Marshal.StringToHGlobalAnsi(deviceExtensions[i]);

        var queueInfosArr = queueCreateInfos.ToArray();
        var deviceCreateInfo = new DeviceCreateInfo
        {
            SType = StructureType.DeviceCreateInfo,
            QueueCreateInfoCount = (uint)queueInfosArr.Length,
            EnabledExtensionCount = (uint)deviceExtensions.Count
        };

        fixed (DeviceQueueCreateInfo* pQueueInfos = queueInfosArr)
        fixed (IntPtr* pExt = deviceExtPtrs)
        {
            deviceCreateInfo.PQueueCreateInfos = pQueueInfos;
            deviceCreateInfo.PpEnabledExtensionNames = (byte**)pExt;
            fixed (Device* pDevice = &_device)
                _vk.CreateDevice(_physicalDevice, &deviceCreateInfo, null, pDevice).Check();
        }

        _vk.GetDeviceQueue(_device, _graphicsQueueFamilyIndex, 0, out _graphicsQueue);
        if (_computeQueueFamilyIndex != _graphicsQueueFamilyIndex)
            _vk.GetDeviceQueue(_device, _computeQueueFamilyIndex, 0, out _computeQueue);
        else
            _computeQueue = _graphicsQueue;

        Console.WriteLine($"[Vulkan] Device ready. Graphics queue family: {_graphicsQueueFamilyIndex}");
    }

    private HashSet<string> GetAvailableInstanceExtensions()
    {
        uint count = 0;
        _vk.EnumerateInstanceExtensionProperties((byte*)null, &count, null);
        var extensions = new ExtensionProperties[count];
        fixed (ExtensionProperties* pExt = extensions)
            _vk.EnumerateInstanceExtensionProperties((byte*)null, &count, pExt);

        var names = new HashSet<string>();
        for (int i = 0; i < count; i++)
        {
            fixed (byte* pName = extensions[i].ExtensionName)
                names.Add(Marshal.PtrToStringAnsi((IntPtr)pName)!);
        }
        return names;
    }

    private HashSet<string> GetAvailableDeviceExtensions()
    {
        uint count = 0;
        _vk.EnumerateDeviceExtensionProperties(_physicalDevice, (byte*)null, &count, null);
        var extensions = new ExtensionProperties[count];
        fixed (ExtensionProperties* pExt = extensions)
            _vk.EnumerateDeviceExtensionProperties(_physicalDevice, (byte*)null, &count, pExt);

        var names = new HashSet<string>();
        for (int i = 0; i < count; i++)
        {
            fixed (byte* pName = extensions[i].ExtensionName)
                names.Add(Marshal.PtrToStringAnsi((IntPtr)pName)!);
        }
        return names;
    }

    private static unsafe string GetFixedString(byte* fixedBuffer)
    {
        return Marshal.PtrToStringAnsi((IntPtr)fixedBuffer) ?? string.Empty;
    }

    private PhysicalDevice PickPhysicalDevice(PhysicalDevice[] devices)
    {
        foreach (var device in devices)
        {
            _vk.GetPhysicalDeviceProperties(device, out var props);
            if (props.DeviceType == PhysicalDeviceType.DiscreteGpu)
                return device;
        }
        foreach (var device in devices)
        {
            _vk.GetPhysicalDeviceProperties(device, out var props);
            if (props.DeviceType == PhysicalDeviceType.IntegratedGpu)
                return device;
        }
        return devices.Length > 0 ? devices[0] : default;
    }

    public void Dispose()
    {
        if (_disposed) return;
        _disposed = true;

        if (_device.Handle != 0)
            _vk.DestroyDevice(_device, null);

        if (_instance.Handle != 0)
            _vk.DestroyInstance(_instance, null);

        _vk.Dispose();
        Console.WriteLine("[Vulkan] Device disposed.");
    }
}

internal static class VkResultExtensions
{
    public static void Check(this Result result)
    {
        if (result != Result.Success)
            throw new InvalidOperationException($"Vulkan error: {result}");
    }
}
