using Avalonia.Controls;
using Avalonia.Controls.Templates;
using AvaloniaMedia.ViewModels;

namespace AvaloniaMedia.Views;

public partial class MediaLibrary : UserControl
{
    private static readonly FuncTemplate<Panel?> WrapPanelTemplate = new(() => new WrapPanel());
    private static readonly FuncTemplate<Panel?> VirtualizingPanelTemplate = new(() => new VirtualizingStackPanel());

    public MediaLibrary()
    {
        InitializeComponent();
        DataContextChanged += OnDataContextChanged;
    }

    private void OnDataContextChanged(object? sender, System.EventArgs e)
    {
        if (DataContext is MediaLibraryViewModel vm)
        {
            vm.PropertyChanged += (s, args) =>
            {
                if (args.PropertyName == nameof(MediaLibraryViewModel.ViewMode))
                    ApplyViewMode(vm.ViewMode);
            };
            ApplyViewMode(vm.ViewMode);
        }
    }

    private void ApplyViewMode(MediaViewMode mode)
    {
        MediaItemsControl.ItemsPanel = mode == MediaViewMode.List
            ? VirtualizingPanelTemplate
            : WrapPanelTemplate;
    }
}
