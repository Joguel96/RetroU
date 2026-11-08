import React, { useState, useEffect } from 'react';
import { 
  Palette, 
  Gamepad2, 
  Sliders, 
  Sparkles, 
  Sun, 
  Moon, 
  Volume2, 
  VolumeX, 
  Check, 
  CheckCircle2, 
  Eye, 
  Download, 
  DownloadCloud, 
  Play, 
  X, 
  Cpu, 
  Trash2, 
  HardDrive, 
  RefreshCw, 
  StopCircle,
  ChevronRight,
  Music,
  Radio
} from 'lucide-react';
import { useRetro, SettingsTab } from '../context/RetroContext';
import { 
  ACCENT_COLORS, 
  WALLPAPERS, 
  CONTROLLER_PRESETS 
} from '../data/mockData';
import { ControllerPresetId } from '../types';
import { soundManager } from '../utils/audio';

export const SettingsView: React.FC = () => {
  const { 
    theme, 
    activeAccent, 
    setAccentColor, 
    setGlassOpacity, 
    setGlassBlur, 
    setColorMode, 
    setWallpaper, 
    setSoundEnabled, 
    setCrtEffect,
    controllerMapping,
    setPreset,
    updateBinding,
    cores,
    coreDownloadState,
    downloadCore,
    downloadAllCores,
    deleteCore,
    cancelCoreDownload,
    settingsTab: activeTab,
    setSettingsTab: setActiveTab
  } = useRetro();

  type CoreFilter = 'all' | 'installed' | 'uninstalled' | 'update';
  const [coreFilter, setCoreFilter] = useState<CoreFilter>('all');
  const [listeningButton, setListeningButton] = useState<string | null>(null);
  const [activePressedKey, setActivePressedKey] = useState<string | null>(null);
  const [testerSuccess, setTesterSuccess] = useState<string | null>(null);

  // Key listening effect for remapping buttons
  useEffect(() => {
    if (!listeningButton) return;

    const handleKeyDown = (e: KeyboardEvent) => {
      e.preventDefault();
      e.stopPropagation();

      const newKey = e.key === ' ' ? 'Space' : e.key;
      updateBinding(listeningButton, newKey);
      setTesterSuccess(`Botón asignado a: ${newKey}`);
      setListeningButton(null);
      setTimeout(() => setTesterSuccess(null), 2500);
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [listeningButton, updateBinding]);

  // Live input tester detection
  useEffect(() => {
    const handleTestKey = (e: KeyboardEvent) => {
      const keyName = e.key === ' ' ? 'Space' : e.key;
      setActivePressedKey(keyName);
      soundManager.playNavTick();
    };

    const handleKeyUp = () => {
      setActivePressedKey(null);
    };

    window.addEventListener('keydown', handleTestKey);
    window.addEventListener('keyup', handleKeyUp);
    return () => {
      window.removeEventListener('keydown', handleTestKey);
      window.removeEventListener('keyup', handleKeyUp);
    };
  }, []);

  // Standard controller buttons list
  const controllerButtons = [
    { id: 'dpad-up', label: 'D-Pad Arriba', group: 'Cruceta Direccional' },
    { id: 'dpad-down', label: 'D-Pad Abajo', group: 'Cruceta Direccional' },
    { id: 'dpad-left', label: 'D-Pad Izquierda', group: 'Cruceta Direccional' },
    { id: 'dpad-right', label: 'D-Pad Derecha', group: 'Cruceta Direccional' },
    { id: 'btn-a', label: 'Botón A (Confirmar)', group: 'Botones Principales' },
    { id: 'btn-b', label: 'Botón B (Cancelar)', group: 'Botones Principales' },
    { id: 'btn-x', label: 'Botón X (Acción)', group: 'Botones Principales' },
    { id: 'btn-y', label: 'Botón Y (Filtro)', group: 'Botones Principales' },
    { id: 'btn-l', label: 'Gatillo L1', group: 'Gatillos Superiores' },
    { id: 'btn-r', label: 'Gatillo R1', group: 'Gatillos Superiores' },
    { id: 'btn-zl', label: 'Gatillo ZL / L2', group: 'Gatillos Superiores' },
    { id: 'btn-zr', label: 'Gatillo ZR / R2', group: 'Gatillos Superiores' },
    { id: 'btn-plus', label: 'Start (+)', group: 'Sistema' },
    { id: 'btn-minus', label: 'Select (-)', group: 'Sistema' },
    { id: 'btn-home', label: 'Botón HOME', group: 'Sistema' },
    { id: 'left-stick', label: 'Stick Analógico Izq.', group: 'Palancas' },
    { id: 'right-stick', label: 'Stick Analógico Der.', group: 'Palancas' },
  ];

  // Filtered cores list (exact 4 options: todos/instalados/no instalados/actualizar)
  const filteredCores = cores.filter(c => {
    if (coreFilter === 'installed') return c.isInstalled;
    if (coreFilter === 'uninstalled') return !c.isInstalled;
    if (coreFilter === 'update') return c.isInstalled && c.updateAvailable;
    return true; // 'all'
  });

  // Sidebar navigation sections: Personalizar and Configuración
  const sidebarItems: Array<{
    id: SettingsTab;
    label: string;
    subtitle: string;
    group: 'personalizar' | 'configuracion';
    icon: React.FC<{ className?: string }>;
    badge?: string;
  }> = [
    {
      id: 'theme',
      label: 'Tema',
      subtitle: 'Colores, cristal y fondos',
      group: 'personalizar',
      icon: Palette,
    },
    {
      id: 'cores',
      label: 'Cores',
      subtitle: 'Núcleos de emulación',
      group: 'configuracion',
      icon: Cpu,
      badge: `${cores.filter(c => c.isInstalled).length}/${cores.length}`,
    },
    {
      id: 'controls',
      label: 'Controles',
      subtitle: 'Mapeo y calibración',
      group: 'configuracion',
      icon: Gamepad2,
    },
    {
      id: 'sound',
      label: 'Sonido',
      subtitle: 'Efectos y volumen',
      group: 'configuracion',
      icon: Volume2,
      badge: theme.soundEnabled ? 'ON' : 'OFF',
    },
  ];

  const handleSelectSection = (sectionId: SettingsTab) => {
    soundManager.playSelect();
    setActiveTab(sectionId);
  };

  // Keyboard navigation for sidebar items
  useEffect(() => {
    if (listeningButton) return;

    const handleSidebarKeys = (e: KeyboardEvent) => {
      const target = e.target as HTMLElement;
      if (target.tagName === 'INPUT' || target.tagName === 'SELECT' || target.tagName === 'TEXTAREA') {
        return;
      }

      if (e.key === 'ArrowUp') {
        e.preventDefault();
        soundManager.playNavTick();
        const currentIdx = sidebarItems.findIndex(item => item.id === activeTab);
        const prevIdx = (currentIdx - 1 + sidebarItems.length) % sidebarItems.length;
        setActiveTab(sidebarItems[prevIdx].id);
      } else if (e.key === 'ArrowDown') {
        e.preventDefault();
        soundManager.playNavTick();
        const currentIdx = sidebarItems.findIndex(item => item.id === activeTab);
        const nextIdx = (currentIdx + 1) % sidebarItems.length;
        setActiveTab(sidebarItems[nextIdx].id);
      } else if (e.key === 'a' || e.key === 'A' || e.key === 'Enter') {
        soundManager.playSelect();
      }
    };

    window.addEventListener('keydown', handleSidebarKeys);
    return () => window.removeEventListener('keydown', handleSidebarKeys);
  }, [activeTab, listeningButton, setActiveTab, sidebarItems]);

  return (
    <div className="flex flex-col md:flex-row gap-6 pb-28 sm:pb-36 animate-in fade-in duration-300 items-start">
      {/* ======================================================== */}
      {/* LEFT SIDEBAR: Personalizar & Configuración Navigation    */}
      {/* ======================================================== */}
      <aside className="w-full md:w-64 lg:w-72 shrink-0 glass-panel rounded-3xl p-4 border border-white/15 shadow-2xl bg-slate-950/60 backdrop-blur-2xl md:sticky md:top-4 space-y-5 select-none">
        {/* Sidebar Header */}
        <div className="flex items-center gap-2.5 px-3 py-2 border-b border-white/10">
          <span 
            className="w-2.5 h-2.5 rounded-full shadow-sm" 
            style={{ backgroundColor: activeAccent.hex }} 
          />
          <span className="text-xs font-black tracking-wider uppercase text-white">
            Ajustes del Sistema
          </span>
        </div>

        {/* GROUP 1: PERSONALIZAR */}
        <div className="space-y-1.5">
          <div className="px-3 py-1 flex items-center justify-between text-[11px] font-mono font-bold tracking-wider uppercase text-slate-400">
            <span>Personalizar</span>
            <span className="text-[10px] text-slate-500 font-sans font-normal">Interfaz</span>
          </div>

          <div className="space-y-1">
            {sidebarItems
              .filter(item => item.group === 'personalizar')
              .map(item => {
                const Icon = item.icon;
                const isActive = activeTab === item.id;
                return (
                  <button
                    key={item.id}
                    onClick={() => handleSelectSection(item.id)}
                    className={`w-full flex items-center justify-between p-3 rounded-2xl transition-all cursor-pointer text-left group ${
                      isActive
                        ? 'bg-white/15 text-white border shadow-md'
                        : 'text-slate-300 hover:text-white hover:bg-white/5 border border-transparent'
                    }`}
                    style={
                      isActive
                        ? {
                            backgroundColor: activeAccent.subtle,
                            borderColor: activeAccent.border,
                          }
                        : {}
                    }
                  >
                    <div className="flex items-center gap-3 min-w-0">
                      <div 
                        className={`p-2 rounded-xl transition-colors shrink-0 ${
                          isActive ? 'bg-white/20 text-white' : 'bg-black/30 text-slate-400 group-hover:text-white'
                        }`}
                        style={isActive ? { color: activeAccent.hex } : {}}
                      >
                        <Icon className="w-4 h-4" />
                      </div>
                      <div className="min-w-0">
                        <span className="text-xs font-bold block truncate text-white">
                          {item.label}
                        </span>
                        <span className="text-[10px] text-slate-400 block truncate">
                          {item.subtitle}
                        </span>
                      </div>
                    </div>

                    <div className="flex items-center gap-2 shrink-0">
                      {isActive && (
                        <div 
                          className="w-1.5 h-5 rounded-full shadow-sm" 
                          style={{ backgroundColor: activeAccent.hex }} 
                        />
                      )}
                      <ChevronRight className={`w-3.5 h-3.5 transition-transform ${isActive ? 'text-white translate-x-0.5' : 'text-slate-500'}`} />
                    </div>
                  </button>
                );
              })}
          </div>
        </div>

        {/* GROUP 2: CONFIGURACIÓN */}
        <div className="space-y-1.5 pt-2 border-t border-white/10">
          <div className="px-3 py-1 flex items-center justify-between text-[11px] font-mono font-bold tracking-wider uppercase text-slate-400">
            <span>Configuración</span>
            <span className="text-[10px] text-slate-500 font-sans font-normal">Sistema</span>
          </div>

          <div className="space-y-1">
            {sidebarItems
              .filter(item => item.group === 'configuracion')
              .map(item => {
                const Icon = item.icon;
                const isActive = activeTab === item.id;
                return (
                  <button
                    key={item.id}
                    onClick={() => handleSelectSection(item.id)}
                    className={`w-full flex items-center justify-between p-3 rounded-2xl transition-all cursor-pointer text-left group ${
                      isActive
                        ? 'bg-white/15 text-white border shadow-md'
                        : 'text-slate-300 hover:text-white hover:bg-white/5 border border-transparent'
                    }`}
                    style={
                      isActive
                        ? {
                            backgroundColor: activeAccent.subtle,
                            borderColor: activeAccent.border,
                          }
                        : {}
                    }
                  >
                    <div className="flex items-center gap-3 min-w-0">
                      <div 
                        className={`p-2 rounded-xl transition-colors shrink-0 ${
                          isActive ? 'bg-white/20 text-white' : 'bg-black/30 text-slate-400 group-hover:text-white'
                        }`}
                        style={isActive ? { color: activeAccent.hex } : {}}
                      >
                        <Icon className="w-4 h-4" />
                      </div>
                      <div className="min-w-0">
                        <span className="text-xs font-bold block truncate text-white">
                          {item.label}
                        </span>
                        <span className="text-[10px] text-slate-400 block truncate">
                          {item.subtitle}
                        </span>
                      </div>
                    </div>

                    <div className="flex items-center gap-2 shrink-0">
                      {item.badge && (
                        <span className={`px-2 py-0.5 rounded-md text-[10px] font-mono font-bold ${
                          isActive 
                            ? 'bg-white/20 text-white' 
                            : 'bg-black/40 text-slate-300'
                        }`}>
                          {item.badge}
                        </span>
                      )}
                      {isActive && (
                        <div 
                          className="w-1.5 h-5 rounded-full shadow-sm" 
                          style={{ backgroundColor: activeAccent.hex }} 
                        />
                      )}
                      <ChevronRight className={`w-3.5 h-3.5 transition-transform ${isActive ? 'text-white translate-x-0.5' : 'text-slate-500'}`} />
                    </div>
                  </button>
                );
              })}
          </div>
        </div>

        {/* Sidebar Footer Console Hints */}
        <div className="pt-2 border-t border-white/10 px-2 flex items-center justify-between text-[10px] font-mono text-slate-400">
          <span>▲ / ▼ Navegar</span>
          <span className="text-white font-bold bg-white/10 px-1.5 py-0.5 rounded">(A) Seleccionar</span>
        </div>
      </aside>

      {/* ======================================================== */}
      {/* RIGHT CONTENT PANEL: Selected Section Configurations    */}
      {/* ======================================================== */}
      <section className="flex-1 min-w-0 w-full space-y-6">
        {/* ==================================================== */}
        {/* 1. TEMA (PERSONALIZAR)                               */}
        {/* ==================================================== */}
        {activeTab === 'theme' && (
          <div className="space-y-6 animate-in fade-in duration-200">
            {/* Section Header */}
            <div className="p-6 rounded-3xl glass-panel border border-white/15 space-y-1">
              <div className="flex items-center gap-2.5">
                <Palette className="w-5 h-5" style={{ color: activeAccent.hex }} />
                <h2 className="text-base sm:text-lg font-bold text-white tracking-tight">
                  Personalización de Tema y Cristal
                </h2>
              </div>
              <p className="text-xs text-slate-400">
                Ajusta el color de acento, reflejos y transparencia del cristal, filtros visuales y fondos de pantalla.
              </p>
            </div>

            {/* 1. Accent Color Selector */}
            <div className="p-6 rounded-3xl glass-panel border border-white/15 space-y-4">
              <div className="flex items-center justify-between">
                <div>
                  <h3 className="text-sm font-bold text-white flex items-center gap-2">
                    <Sparkles className="w-4 h-4" style={{ color: activeAccent.hex }} />
                    <span>Color de Acento de la Interfaz</span>
                  </h3>
                  <p className="text-xs text-slate-400 mt-0.5">
                    Modifica los reflejos, bordes activos y luces del cristal de la consola.
                  </p>
                </div>
                <button
                  onClick={() => {
                    document.getElementById('wallpaper-gallery')?.scrollIntoView({ behavior: 'smooth' });
                  }}
                  className="flex items-center gap-1.5 px-3 py-1.5 rounded-xl glass-button text-[11px] font-medium text-slate-300 hover:text-white cursor-pointer"
                >
                  <Eye className="w-3.5 h-3.5" />
                  <span>Ver Fondos ↓</span>
                </button>
              </div>

              {/* Accent Color Palette Chips */}
              <div className="grid grid-cols-2 sm:grid-cols-4 md:grid-cols-8 gap-3">
                {ACCENT_COLORS.map(acc => {
                  const isSelected = acc.id === theme.accentId;
                  return (
                    <button
                      key={acc.id}
                      onClick={() => setAccentColor(acc.id)}
                      className={`relative flex flex-col items-center justify-center p-3 rounded-2xl transition-all cursor-pointer group ${
                        isSelected 
                          ? 'ring-2 ring-white/50 scale-105 shadow-xl' 
                          : 'hover:scale-102 hover:bg-white/5'
                      }`}
                      style={{
                        backgroundColor: isSelected ? acc.subtle : 'rgba(255, 255, 255, 0.04)',
                        borderColor: isSelected ? acc.border : 'rgba(255, 255, 255, 0.1)',
                        borderWidth: '1px',
                      }}
                    >
                      <div 
                        className="w-8 h-8 rounded-full shadow-md flex items-center justify-center transition-transform group-hover:scale-110 mb-2"
                        style={{ backgroundColor: acc.hex }}
                      >
                        {isSelected && <Check className="w-4 h-4 text-white drop-shadow-sm" />}
                      </div>
                      <span className="text-[11px] font-medium text-slate-200 text-center line-clamp-1">
                        {acc.name}
                      </span>
                    </button>
                  );
                })}
              </div>
            </div>

            {/* 2. Glassmorphism Sliders */}
            <div className="p-6 rounded-3xl glass-panel border border-white/15 space-y-6">
              <h3 className="text-sm font-bold text-white flex items-center gap-2">
                <Sliders className="w-4 h-4" style={{ color: activeAccent.hex }} />
                <span>Propiedades del Cristal Traslúcido (Glassmorphism)</span>
              </h3>

              <div className="space-y-5">
                {/* Blur Slider */}
                <div className="space-y-2">
                  <div className="flex justify-between text-xs">
                    <span className="text-slate-300 font-medium">Desenfoque de Fondo (Backdrop Blur)</span>
                    <span className="font-mono text-white font-bold">{theme.glassBlur}px</span>
                  </div>
                  <input
                    type="range"
                    min="4"
                    max="32"
                    step="2"
                    value={theme.glassBlur}
                    onChange={(e) => setGlassBlur(Number(e.target.value))}
                    className="w-full h-1.5 bg-white/20 rounded-lg appearance-none cursor-pointer accent-white"
                  />
                  <div className="flex justify-between text-[10px] font-mono text-slate-500">
                    <span>4px (Sutil)</span>
                    <span>16px (Equilibrado)</span>
                    <span>32px (Máximo Cristal)</span>
                  </div>
                </div>

                {/* Opacity Slider */}
                <div className="space-y-2">
                  <div className="flex justify-between text-xs">
                    <span className="text-slate-300 font-medium">Opacidad de la Superficie de Cristal</span>
                    <span className="font-mono text-white font-bold">{Math.round(theme.glassOpacity * 100)}%</span>
                  </div>
                  <input
                    type="range"
                    min="0.06"
                    max="0.45"
                    step="0.02"
                    value={theme.glassOpacity}
                    onChange={(e) => setGlassOpacity(Number(e.target.value))}
                    className="w-full h-1.5 bg-white/20 rounded-lg appearance-none cursor-pointer accent-white"
                  />
                  <div className="flex justify-between text-[10px] font-mono text-slate-500">
                    <span>6% (Ultra Translúcido)</span>
                    <span>12% (Estándar RetroU)</span>
                    <span>45% (Opaco Mate)</span>
                  </div>
                </div>

                {/* Toggles */}
                <div className="grid grid-cols-1 sm:grid-cols-2 gap-3 pt-2">
                  {/* Dark / Light Toggle */}
                  <button
                    onClick={() => setColorMode(theme.colorMode === 'dark' ? 'light' : 'dark')}
                    className="flex items-center justify-between p-3.5 rounded-2xl glass-card hover:bg-white/10 transition-all cursor-pointer"
                  >
                    <span className="text-xs font-semibold text-white">Modo Base</span>
                    <span className="text-xs font-mono font-bold text-slate-300 flex items-center gap-1">
                      {theme.colorMode === 'dark' ? <Moon className="w-3.5 h-3.5 text-indigo-300" /> : <Sun className="w-3.5 h-3.5 text-amber-300" />}
                      {theme.colorMode === 'dark' ? 'Oscuro' : 'Claro'}
                    </span>
                  </button>

                  {/* CRT Scanlines Toggle */}
                  <button
                    onClick={() => setCrtEffect(!theme.crtEffect)}
                    className={`flex items-center justify-between p-3.5 rounded-2xl glass-card transition-all cursor-pointer ${
                      theme.crtEffect ? 'border-white/30 bg-white/10' : ''
                    }`}
                  >
                    <span className="text-xs font-semibold text-white">Filtro CRT Scanlines</span>
                    <span className={`text-xs font-mono font-bold ${theme.crtEffect ? 'text-white' : 'text-slate-500'}`}>
                      {theme.crtEffect ? 'ON' : 'OFF'}
                    </span>
                  </button>
                </div>
              </div>
            </div>

            {/* 3. Wallpaper Gallery (Spanning full width!) */}
            <div id="wallpaper-gallery" className="p-6 rounded-3xl glass-panel border border-white/15 space-y-4 scroll-mt-6">
              <div className="flex items-center justify-between">
                <div>
                  <h3 className="text-sm font-bold text-white flex items-center gap-2">
                    <Eye className="w-4 h-4" style={{ color: activeAccent.hex }} />
                    <span>Fondos de Pantalla de la Consola</span>
                  </h3>
                  <p className="text-xs text-slate-400 mt-0.5">
                    Fondos abstractos de cristal con cambio dinámico al pasar el cursor sobre un juego (estilo PS3 / PS4).
                  </p>
                </div>
                <span className="text-xs font-mono text-slate-400">
                  {WALLPAPERS.length} temas disponibles
                </span>
              </div>

              <div className="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-4 gap-4">
                {WALLPAPERS.map(wp => {
                  const isSelected = wp.id === theme.wallpaperId;
                  return (
                    <button
                      key={wp.id}
                      onClick={() => setWallpaper(wp.id)}
                      className={`relative flex flex-col text-left p-3 rounded-2xl glass-card transition-all cursor-pointer ${
                        isSelected ? 'ring-2 ring-white/50 scale-102' : 'hover:scale-101'
                      }`}
                      style={{
                        borderColor: isSelected ? activeAccent.hex : 'rgba(255,255,255,0.1)',
                      }}
                    >
                      <div className={`aspect-[16/9] w-full rounded-xl mb-2 bg-gradient-to-br ${wp.previewGradient} border border-white/10 overflow-hidden relative shadow-inner`}>
                        {isSelected && (
                          <div className="absolute top-2 right-2 p-1 rounded-full bg-black/60 text-emerald-400">
                            <Check className="w-3 h-3" />
                          </div>
                        )}
                      </div>
                      <span className="text-xs font-bold text-white">{wp.name}</span>
                      <span className="text-[10px] text-slate-400 line-clamp-1">{wp.description}</span>
                    </button>
                  );
                })}
              </div>
            </div>
          </div>
        )}

        {/* ==================================================== */}
        {/* 2. CORES (CONFIGURACIÓN)                             */}
        {/* ==================================================== */}
        {activeTab === 'cores' && (
          <div className="space-y-6 animate-in fade-in duration-200">
            {/* Header & Global Download Bar */}
            <div className="p-6 rounded-3xl glass-panel border border-white/15 space-y-5">
              <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
                <div className="flex items-center gap-3.5">
                  <div 
                    className="w-12 h-12 rounded-2xl flex items-center justify-center glass-tile text-white shadow-xl shrink-0"
                    style={{ borderColor: activeAccent.hex }}
                  >
                    <Cpu className="w-6 h-6" style={{ color: activeAccent.hex }} />
                  </div>
                  <div>
                    <h2 className="text-base sm:text-lg font-bold text-white flex items-center gap-2">
                      <span>Gestor de Núcleos de Emulación (Cores)</span>
                      <span className="px-2 py-0.5 rounded-full text-[10px] font-mono bg-emerald-500/20 text-emerald-300 border border-emerald-500/30">
                        Libretro v1.17
                      </span>
                    </h2>
                    <p className="text-xs text-slate-400 mt-0.5">
                      Descarga cores individuales o todos a la vez. Almacenamiento local en <code className="text-slate-300 font-mono">/sdcard/retro_u/cores/</code>
                    </p>
                  </div>
                </div>

                {/* Global "Descargar Todos los Cores" Button */}
                <div className="flex items-center gap-2.5 shrink-0">
                  {coreDownloadState.isDownloading && coreDownloadState.coreId === 'all' ? (
                    <button
                      onClick={cancelCoreDownload}
                      className="flex items-center gap-2 px-4 py-2.5 rounded-xl text-xs font-semibold bg-rose-500/20 hover:bg-rose-500/30 border border-rose-500/30 text-rose-200 transition-colors cursor-pointer"
                    >
                      <StopCircle className="w-4 h-4" />
                      <span>Cancelar Descarga ({coreDownloadState.progress}%)</span>
                    </button>
                  ) : (
                    <button
                      onClick={downloadAllCores}
                      disabled={cores.every(c => c.isInstalled && !c.updateAvailable)}
                      className={`flex items-center gap-2 px-5 py-2.5 rounded-xl text-xs font-bold transition-all shadow-xl cursor-pointer ${
                        cores.every(c => c.isInstalled && !c.updateAvailable)
                          ? 'bg-white/10 text-slate-400 cursor-not-allowed border border-white/10'
                          : 'bg-white text-slate-900 hover:bg-slate-100 active:scale-95'
                      }`}
                      style={
                        !cores.every(c => c.isInstalled && !c.updateAvailable)
                          ? {
                              boxShadow: 'inset 0 1px 1px rgba(255,255,255,0.4), 0 8px 20px rgba(0,0,0,0.4)',
                            }
                          : {}
                      }
                    >
                      <DownloadCloud className="w-4 h-4" />
                      <span>
                        {cores.every(c => c.isInstalled && !c.updateAvailable)
                          ? 'Todos los Cores Están Instalados'
                          : 'Descargar Todos los Cores'}
                      </span>
                    </button>
                  )}
                </div>
              </div>

              {/* Global Download Progress Bar when downloading ALL */}
              {coreDownloadState.isDownloading && (
                <div className="p-4 rounded-2xl bg-slate-950/60 border border-white/15 space-y-2 animate-in fade-in">
                  <div className="flex items-center justify-between text-xs font-mono">
                    <span className="text-white font-semibold flex items-center gap-2">
                      <RefreshCw className="w-3.5 h-3.5 animate-spin text-cyan-400" />
                      Descargando: {coreDownloadState.currentCoreName}
                    </span>
                    <div className="flex items-center gap-3 text-slate-300">
                      <span>{coreDownloadState.speed}</span>
                      <span>{coreDownloadState.downloadedBytes}</span>
                      <span className="font-bold text-white">{coreDownloadState.progress}%</span>
                    </div>
                  </div>
                  <div className="w-full h-2 rounded-full bg-white/10 overflow-hidden">
                    <div 
                      className="h-full rounded-full transition-all duration-300 shadow-sm"
                      style={{ 
                        width: `${coreDownloadState.progress}%`,
                        backgroundColor: activeAccent.hex 
                      }}
                    />
                  </div>
                </div>
              )}
            </div>

            {/* 4-Option Filter Bar (Without search input) */}
            <div className="p-3 sm:p-4 rounded-3xl glass-panel border border-white/15 flex items-center gap-2 overflow-x-auto">
              {[
                { id: 'all' as const, label: 'Todos', count: cores.length },
                { id: 'installed' as const, label: 'Instalados', count: cores.filter(c => c.isInstalled).length },
                { id: 'uninstalled' as const, label: 'No instalados', count: cores.filter(c => !c.isInstalled).length },
                { id: 'update' as const, label: 'Actualizar', count: cores.filter(c => c.isInstalled && c.updateAvailable).length },
              ].map(tab => {
                const isActive = coreFilter === tab.id;
                return (
                  <button
                    key={tab.id}
                    onClick={() => {
                      soundManager.playNavTick();
                      setCoreFilter(tab.id);
                    }}
                    className={`flex items-center gap-2 px-4 py-2 text-xs font-semibold rounded-2xl whitespace-nowrap transition-all cursor-pointer ${
                      isActive
                        ? 'bg-white/20 text-white border border-white/30 shadow-md'
                        : 'text-slate-400 hover:text-white hover:bg-white/5 border border-transparent'
                    }`}
                    style={
                      isActive
                        ? {
                            backgroundColor: activeAccent.subtle,
                            borderColor: activeAccent.border,
                          }
                        : {}
                    }
                  >
                    <span>{tab.label}</span>
                    <span className={`px-2 py-0.5 rounded-full text-[10px] font-mono font-bold ${
                      isActive ? 'bg-white/20 text-white' : 'bg-black/40 text-slate-400'
                    }`}>
                      {tab.count}
                    </span>
                  </button>
                );
              })}
            </div>

            {/* Cores List (Vertical list layout) */}
            <div className="flex flex-col gap-3">
              {filteredCores.length > 0 ? (
                filteredCores.map(core => {
                  const isThisCoreDownloading = coreDownloadState.isDownloading && 
                    (coreDownloadState.coreId === core.id || coreDownloadState.coreId === 'all');

                  return (
                    <div
                      key={core.id}
                      className="p-4 sm:p-4.5 rounded-2xl glass-card border border-white/10 flex flex-col md:flex-row md:items-center justify-between gap-4 transition-all hover:border-white/20 hover:bg-white/5"
                    >
                      {/* Left: Badge, Core Name, Version, Platform & Description */}
                      <div className="flex items-center gap-3.5 min-w-0 flex-1">
                        <div className="w-11 h-11 rounded-xl bg-black/40 border border-white/15 flex items-center justify-center shrink-0 text-white font-mono font-bold text-xs shadow-inner">
                          {core.consoleId.toUpperCase().slice(0, 4)}
                        </div>

                        <div className="min-w-0 flex-1">
                          <div className="flex items-center flex-wrap gap-2">
                            <h3 className="font-bold text-sm text-white truncate">{core.name}</h3>
                            <span className="text-[10px] font-mono px-2 py-0.5 rounded-full bg-white/10 text-slate-300">
                              v{core.version}
                            </span>
                            <span className="text-[11px] font-mono text-slate-400">
                              {core.fileSize}
                            </span>
                            <span className="text-[11px] font-mono text-slate-500 hidden lg:inline">
                              ({core.id}.so)
                            </span>
                          </div>

                          <div className="flex items-center gap-2 mt-0.5 text-xs text-slate-300">
                            <span className="font-medium text-slate-200">{core.systemName}</span>
                            <span className="text-slate-600">·</span>
                            <span className="text-[11px] text-slate-400 truncate">{core.description}</span>
                          </div>
                        </div>
                      </div>

                      {/* Right: Status badge & Actions */}
                      <div className="flex items-center justify-between md:justify-end gap-3 shrink-0 pt-2 md:pt-0 border-t md:border-t-0 border-white/10">
                        {/* Status Badge */}
                        <div className="shrink-0">
                          {core.isInstalled ? (
                            core.updateAvailable ? (
                              <div className="flex items-center gap-1.5 px-2.5 py-1 rounded-lg bg-amber-500/20 border border-amber-400/40 text-amber-300 text-[10px] font-mono font-bold">
                                <RefreshCw className="w-3 h-3" />
                                <span>ACTUALIZABLE</span>
                              </div>
                            ) : (
                              <div className="flex items-center gap-1.5 px-2.5 py-1 rounded-lg bg-emerald-500/20 border border-emerald-400/40 text-emerald-300 text-[10px] font-mono font-bold">
                                <Check className="w-3 h-3" />
                                <span>INSTALADO</span>
                              </div>
                            )
                          ) : (
                            <div className="px-2.5 py-1 rounded-lg bg-white/5 border border-white/10 text-slate-400 text-[10px] font-mono">
                              NO INSTALADO
                            </div>
                          )}
                        </div>

                        {/* Action buttons */}
                        <div className="flex items-center gap-2 shrink-0">
                          {core.isInstalled ? (
                            <>
                              {core.updateAvailable && (
                                <button
                                  onClick={() => downloadCore(core.id)}
                                  disabled={coreDownloadState.isDownloading}
                                  className="flex items-center gap-1.5 px-3 py-1.5 rounded-xl text-xs font-semibold bg-amber-500/20 hover:bg-amber-500/30 border border-amber-400/40 text-amber-200 transition-colors cursor-pointer"
                                >
                                  <RefreshCw className="w-3.5 h-3.5" />
                                  <span>Actualizar</span>
                                </button>
                              )}

                              <button
                                onClick={() => deleteCore(core.id)}
                                disabled={coreDownloadState.isDownloading}
                                title="Desinstalar core de la SD"
                                className="p-2 rounded-xl glass-button text-slate-400 hover:text-rose-400 transition-colors cursor-pointer"
                              >
                                <Trash2 className="w-4 h-4" />
                              </button>
                            </>
                          ) : (
                            <button
                              onClick={() => downloadCore(core.id)}
                              disabled={coreDownloadState.isDownloading}
                              className={`flex items-center gap-1.5 px-4 py-1.5 rounded-xl text-xs font-bold transition-all shadow-md cursor-pointer ${
                                coreDownloadState.isDownloading
                                  ? 'bg-white/10 text-slate-500 cursor-not-allowed'
                                  : 'bg-white text-slate-900 hover:bg-slate-200 active:scale-95'
                              }`}
                            >
                              <Download className="w-3.5 h-3.5" />
                              <span>Descargar</span>
                            </button>
                          )}
                        </div>
                      </div>

                      {/* Progress Bar if currently downloading this core */}
                      {isThisCoreDownloading && (
                        <div className="w-full space-y-1.5 pt-2 border-t border-white/10 mt-2">
                          <div className="flex items-center justify-between text-[11px] font-mono text-slate-300">
                            <span className="flex items-center gap-1.5 text-cyan-300">
                              <RefreshCw className="w-3 h-3 animate-spin" /> Descargando binario...
                            </span>
                            <span className="font-bold text-white">{coreDownloadState.progress}%</span>
                          </div>
                          <div className="w-full h-1.5 rounded-full bg-white/10 overflow-hidden">
                            <div 
                              className="h-full rounded-full transition-all duration-300"
                              style={{ 
                                width: `${coreDownloadState.progress}%`,
                                backgroundColor: activeAccent.hex 
                              }}
                            />
                          </div>
                        </div>
                      )}
                    </div>
                  );
                })
              ) : (
                <div className="p-8 text-center rounded-2xl glass-panel border border-white/10 text-slate-400 text-xs font-medium">
                  No hay cores que coincidan con este filtro.
                </div>
              )}
            </div>
          </div>
        )}

        {/* ==================================================== */}
        {/* 3. CONTROLES (CONFIGURACIÓN)                         */}
        {/* ==================================================== */}
        {activeTab === 'controls' && (
          <div className="space-y-6 animate-in fade-in duration-200">
            {/* Section Header */}
            <div className="p-6 rounded-3xl glass-panel border border-white/15 space-y-1">
              <div className="flex items-center gap-2.5">
                <Gamepad2 className="w-5 h-5" style={{ color: activeAccent.hex }} />
                <h2 className="text-base sm:text-lg font-bold text-white tracking-tight">
                  Mapeo y Calibración de Controles
                </h2>
              </div>
              <p className="text-xs text-slate-400">
                Selecciona perfiles predefinidos de mandos de consola o reasigna cualquier tecla y botón con detección física.
              </p>
            </div>

            {/* Preset Selector Bar */}
            <div className="p-5 rounded-3xl glass-panel border border-white/15 space-y-4 shadow-xl">
              <div className="flex items-center justify-between">
                <div>
                  <h3 className="text-sm font-bold text-white">Preset de Mando Activo</h3>
                  <p className="text-xs text-slate-400">
                    Cambia la distribución de botones según tu periférico físico o preferencia.
                  </p>
                </div>
                <span className="text-xs font-mono text-emerald-400 font-semibold px-2.5 py-1 rounded-full glass-pill">
                  Gamepad API Activa
                </span>
              </div>

              <div className="grid grid-cols-2 sm:grid-cols-3 md:grid-cols-5 gap-3">
                {Object.entries(CONTROLLER_PRESETS).map(([presetId, preset]) => {
                  const isSelected = controllerMapping.preset === presetId;
                  return (
                    <button
                      key={presetId}
                      onClick={() => setPreset(presetId as ControllerPresetId)}
                      className={`p-3 rounded-2xl glass-card transition-all cursor-pointer text-left ${
                        isSelected
                          ? 'ring-2 ring-white/50 bg-white/15 scale-102'
                          : 'hover:bg-white/5'
                      }`}
                      style={
                        isSelected
                          ? {
                              borderColor: activeAccent.hex,
                              boxShadow: `0 0 16px ${activeAccent.subtle}`,
                            }
                          : {}
                      }
                    >
                      <div className="flex items-center justify-between mb-1">
                        <span className="text-xs font-bold text-white">{preset.name}</span>
                        {isSelected && <Check className="w-3.5 h-3.5" style={{ color: activeAccent.hex }} />}
                      </div>
                      <span className="text-[10px] font-mono text-slate-400">Configuración nativa</span>
                    </button>
                  );
                })}
              </div>
            </div>

            {/* Remapping Notification Banner */}
            {listeningButton && (
              <div className="p-4 rounded-2xl bg-amber-500/20 border border-amber-400 text-amber-200 text-xs flex items-center justify-between shadow-xl animate-pulse">
                <span className="font-semibold">
                  ▶ Presiona cualquier tecla en tu teclado o botón de mando para asignar a: <strong>{listeningButton.toUpperCase()}</strong>...
                </span>
                <button
                  onClick={() => setListeningButton(null)}
                  className="px-3 py-1 rounded-lg bg-black/40 hover:bg-black/60 text-white font-mono"
                >
                  Cancelar [Esc]
                </button>
              </div>
            )}

            {testerSuccess && (
              <div className="p-3 rounded-2xl bg-emerald-500/20 border border-emerald-400 text-emerald-200 text-xs flex items-center gap-2">
                <CheckCircle2 className="w-4 h-4" />
                <span>{testerSuccess}</span>
              </div>
            )}

            {/* Interactive Controller Visual Mapping Grid */}
            <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
              {controllerButtons.map(btn => {
                const currentKey = controllerMapping.bindings[btn.id] || 'Sin asignar';
                const isListening = listeningButton === btn.id;
                const isKeyCurrentlyPressed = activePressedKey && currentKey.toLowerCase().includes(activePressedKey.toLowerCase());

                return (
                  <div
                    key={btn.id}
                    className={`p-4 rounded-2xl glass-card flex items-center justify-between transition-all ${
                      isListening ? 'ring-2 ring-amber-400 bg-amber-500/10' : ''
                    } ${isKeyCurrentlyPressed ? 'ring-2 ring-emerald-400 bg-emerald-500/15' : ''}`}
                  >
                    <div>
                      <span className="text-[10px] font-mono text-slate-400 block uppercase tracking-wider">
                        {btn.group}
                      </span>
                      <span className="text-xs font-bold text-white">{btn.label}</span>
                    </div>

                    <div className="flex items-center gap-2">
                      <div 
                        className={`px-3 py-1.5 rounded-xl font-mono text-xs font-bold border transition-colors ${
                          isKeyCurrentlyPressed
                            ? 'bg-emerald-500 text-slate-950 border-emerald-400 shadow-md'
                            : 'bg-black/50 text-cyan-300 border-white/10'
                        }`}
                      >
                        {currentKey}
                      </div>

                      <button
                        onClick={() => setListeningButton(btn.id)}
                        className="px-3 py-1.5 rounded-xl text-xs font-medium text-slate-300 hover:text-white bg-white/5 hover:bg-white/15 border border-white/10 transition-colors cursor-pointer"
                      >
                        {isListening ? 'Esperando...' : 'Remapear'}
                      </button>
                    </div>
                  </div>
                );
              })}
            </div>
          </div>
        )}

        {/* ==================================================== */}
        {/* 4. SONIDO (CONFIGURACIÓN)                            */}
        {/* ==================================================== */}
        {activeTab === 'sound' && (
          <div className="space-y-6 animate-in fade-in duration-200">
            {/* Section Header */}
            <div className="p-6 rounded-3xl glass-panel border border-white/15 space-y-1">
              <div className="flex items-center gap-2.5">
                <Volume2 className="w-5 h-5" style={{ color: activeAccent.hex }} />
                <h2 className="text-base sm:text-lg font-bold text-white tracking-tight">
                  Configuración de Sonido del Sistema
                </h2>
              </div>
              <p className="text-xs text-slate-400">
                Ajusta los efectos sonoros de la interfaz, chimes de consola y prueba el sintetizador de audio en tiempo real.
              </p>
            </div>

            {/* Master Sound Effects Toggle Card */}
            <div className="p-6 rounded-3xl glass-panel border border-white/15 space-y-4">
              <div className="flex flex-col sm:flex-row sm:items-center justify-between gap-4">
                <div className="space-y-1">
                  <h3 className="text-sm font-bold text-white flex items-center gap-2">
                    {theme.soundEnabled ? (
                      <Volume2 className="w-4 h-4 text-emerald-400" />
                    ) : (
                      <VolumeX className="w-4 h-4 text-slate-500" />
                    )}
                    <span>Efectos de Sonido del Menú</span>
                  </h3>
                  <p className="text-xs text-slate-400 max-w-xl">
                    Activa o desactiva la retroalimentación táctil sonora (ticks de navegación, chime de selección, confirmación y boot).
                  </p>
                </div>

                <button
                  onClick={() => {
                    const next = !theme.soundEnabled;
                    setSoundEnabled(next);
                    if (next) soundManager.playSelect();
                  }}
                  className={`flex items-center gap-2.5 px-6 py-3 rounded-2xl text-xs font-bold transition-all shadow-xl cursor-pointer ${
                    theme.soundEnabled
                      ? 'bg-emerald-500/20 text-emerald-300 border border-emerald-400/40'
                      : 'bg-white/10 text-slate-400 border border-white/10 hover:text-white'
                  }`}
                  style={
                    theme.soundEnabled
                      ? {
                          boxShadow: '0 4px 14px rgba(16, 185, 129, 0.25)',
                        }
                      : {}
                  }
                >
                  {theme.soundEnabled && <Check className="w-3.5 h-3.5 text-emerald-400" />}
                  <span>{theme.soundEnabled ? 'SONIDO ACTIVADO' : 'SONIDO SILENCIADO'}</span>
                </button>
              </div>
            </div>

            {/* Interactive Sound Sampler / Chimes Tester */}
            <div className="p-6 rounded-3xl glass-panel border border-white/15 space-y-4">
              <div>
                <h3 className="text-sm font-bold text-white flex items-center gap-2">
                  <Music className="w-4 h-4" style={{ color: activeAccent.hex }} />
                  <span>Probador de Efectos Sonoros (Sintetizador Web Audio API)</span>
                </h3>
                <p className="text-xs text-slate-400 mt-0.5">
                  Haz clic en cada botón para escuchar los efectos acústicos generados por la consola.
                </p>
              </div>

              <div className="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-3 gap-3">
                {[
                  {
                    name: 'Navegación (Tick)',
                    desc: 'Al moverte entre tarjetas y opciones',
                    action: () => soundManager.playNavTick(),
                  },
                  {
                    name: 'Selección (Chime)',
                    desc: 'Al pulsar A o confirmar opción',
                    action: () => soundManager.playSelect(),
                  },
                  {
                    name: 'Retroceso (Atrás)',
                    desc: 'Al pulsar B o cerrar ventana',
                    action: () => soundManager.playBack(),
                  },
                  {
                    name: 'Favorito (Sparkle)',
                    desc: 'Al marcar un juego favorito',
                    action: () => soundManager.playFavorite(),
                  },
                  {
                    name: 'Arranque (Boot)',
                    desc: 'Al lanzar un emulador de juego',
                    action: () => soundManager.playLaunchSound(),
                  },
                  {
                    name: 'Escáner (Blip)',
                    desc: 'Al detectar una nueva ROM en la SD',
                    action: () => soundManager.playScanBlip(),
                  },
                ].map(sound => (
                  <button
                    key={sound.name}
                    onClick={() => {
                      if (!theme.soundEnabled) {
                        setSoundEnabled(true);
                      }
                      sound.action();
                    }}
                    className="p-4 rounded-2xl glass-card border border-white/10 hover:border-white/20 hover:bg-white/10 transition-all text-left group cursor-pointer flex items-center justify-between"
                  >
                    <div>
                      <span className="text-xs font-bold text-white block group-hover:text-cyan-300 transition-colors">
                        {sound.name}
                      </span>
                      <span className="text-[10px] text-slate-400 block mt-0.5">
                        {sound.desc}
                      </span>
                    </div>
                    <div className="p-2 rounded-xl bg-white/5 group-hover:bg-white/15 text-slate-300 group-hover:text-white transition-colors">
                      <Play className="w-3.5 h-3.5 fill-current" />
                    </div>
                  </button>
                ))}
              </div>
            </div>

            {/* Audio Engine Hardware Info */}
            <div className="p-6 rounded-3xl glass-panel border border-white/15 space-y-3">
              <div className="flex items-center gap-2">
                <Radio className="w-4 h-4 text-slate-400" />
                <h3 className="text-sm font-bold text-white">Especificaciones del Motor de Audio</h3>
              </div>

              <div className="grid grid-cols-2 sm:grid-cols-4 gap-3 text-xs font-mono">
                <div className="p-3 rounded-xl bg-black/40 border border-white/10 space-y-1">
                  <span className="text-[10px] text-slate-400 block uppercase">Sintetizador</span>
                  <span className="text-white font-bold block">Web Audio API</span>
                </div>
                <div className="p-3 rounded-xl bg-black/40 border border-white/10 space-y-1">
                  <span className="text-[10px] text-slate-400 block uppercase">Frecuencia</span>
                  <span className="text-white font-bold block">48,000 Hz Stereo</span>
                </div>
                <div className="p-3 rounded-xl bg-black/40 border border-white/10 space-y-1">
                  <span className="text-[10px] text-slate-400 block uppercase">Latencia</span>
                  <span className="text-emerald-400 font-bold block">&lt; 5ms Zero-Buffer</span>
                </div>
                <div className="p-3 rounded-xl bg-black/40 border border-white/10 space-y-1">
                  <span className="text-[10px] text-slate-400 block uppercase">Salida</span>
                  <span className="text-white font-bold block">Altavoces / GamePad</span>
                </div>
              </div>
            </div>
          </div>
        )}
      </section>
    </div>
  );
};
