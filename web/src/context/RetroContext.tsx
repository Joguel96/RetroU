import React, { createContext, useContext, useState, useEffect, useCallback, useRef } from 'react';
import { 
  ConsoleId, 
  RomGame, 
  ThemeSettings, 
  AccentColor, 
  ControllerPresetId, 
  ControllerMapping, 
  ScanState,
  ScanLog,
  EmulatorCore,
  CoreDownloadState 
} from '../types';
import { 
  ACCENT_COLORS, 
  GAMES_DATABASE, 
  CONTROLLER_PRESETS,
  CONSOLES,
  INITIAL_CORES 
} from '../data/mockData';
import { soundManager } from '../utils/audio';

export type ViewMode = 'home' | 'consoles' | 'settings';
export type SettingsTab = 'theme' | 'cores' | 'controls' | 'sound';

interface RetroContextType {
  // Navigation
  currentView: ViewMode;
  setCurrentView: (view: ViewMode) => void;
  settingsTab: SettingsTab;
  setSettingsTab: (tab: SettingsTab) => void;
  selectedConsole: ConsoleId;
  setSelectedConsole: (id: ConsoleId) => void;
  searchQuery: string;
  setSearchQuery: (q: string) => void;

  // Games & Dynamic Wallpaper State (PS3 / PS4 style)
  games: RomGame[];
  hoveredGame: RomGame | null;
  setHoveredGame: (game: RomGame | null) => void;
  focusedGame: RomGame | null;
  setFocusedGame: (game: RomGame | null) => void;
  selectedGame: RomGame | null;
  setSelectedGame: (game: RomGame | null) => void;
  runningGame: RomGame | null;
  setRunningGame: (game: RomGame | null) => void;
  toggleFavorite: (gameId: string) => void;
  focusedIndex: number;
  setFocusedIndex: (idx: number) => void;

  // Themes
  theme: ThemeSettings;
  activeAccent: AccentColor;
  setAccentColor: (accentId: string) => void;
  setGlassOpacity: (opacity: number) => void;
  setGlassBlur: (blur: number) => void;
  setColorMode: (mode: 'dark' | 'light') => void;
  setWallpaper: (wallpaperId: string) => void;
  setSoundEnabled: (enabled: boolean) => void;
  setCrtEffect: (enabled: boolean) => void;

  // Embedded Scanner for Consoles section
  scanState: ScanState;
  startScan: (targetConsole?: ConsoleId) => void;
  cancelScan: () => void;
  showScanModal: boolean;
  setShowScanModal: (show: boolean) => void;

  // Controller
  controllerMapping: ControllerMapping;
  setPreset: (presetId: ControllerPresetId) => void;
  updateBinding: (buttonId: string, key: string) => void;

  // Emulation Cores Manager
  cores: EmulatorCore[];
  coreDownloadState: CoreDownloadState;
  downloadCore: (coreId: string) => void;
  downloadAllCores: () => void;
  deleteCore: (coreId: string) => void;
  cancelCoreDownload: () => void;
}

const RetroContext = createContext<RetroContextType | undefined>(undefined);

const INITIAL_THEME: ThemeSettings = {
  accentId: 'sapphire-blue',
  glassOpacity: 0.12,
  glassBlur: 16,
  colorMode: 'dark',
  wallpaperId: 'slate-minimal',
  soundEnabled: true,
  crtEffect: false,
};

export const RetroProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const [currentView, setCurrentViewRaw] = useState<ViewMode>('home');
  const [settingsTab, setSettingsTab] = useState<SettingsTab>('theme');
  const [selectedConsole, setSelectedConsoleRaw] = useState<ConsoleId>('all');
  const [searchQuery, setSearchQuery] = useState('');
  const [games, setGames] = useState<RomGame[]>(GAMES_DATABASE);
  const [hoveredGame, setHoveredGame] = useState<RomGame | null>(null);
  const [focusedGame, setFocusedGame] = useState<RomGame | null>(GAMES_DATABASE[0] || null);
  const [selectedGame, setSelectedGameRaw] = useState<RomGame | null>(null);
  const [runningGame, setRunningGameRaw] = useState<RomGame | null>(null);
  const [focusedIndex, setFocusedIndex] = useState<number>(0);
  const [showScanModal, setShowScanModal] = useState<boolean>(false);

  const [theme, setTheme] = useState<ThemeSettings>(INITIAL_THEME);
  const [controllerMapping, setControllerMapping] = useState<ControllerMapping>({
    preset: 'wiiu',
    bindings: { ...CONTROLLER_PRESETS['wiiu'].bindings },
  });

  const [scanState, setScanState] = useState<ScanState>({
    isScanning: false,
    progress: 0,
    currentTarget: '/sdcard/roms/',
    currentFile: '',
    foundCount: 0,
    logs: [
      { id: '1', text: 'RetroU Storage Engine listo. Montado en /sdcard/roms/ (31.6 GB libres)', type: 'info', time: '12:00:00' },
      { id: '2', text: 'Estructura fija verificada: /sdcard/roms/[snes|nes|ps1|gba|genesis|n64|arcade|psp]/', type: 'success', time: '12:00:01' },
    ],
    selectedConsoleId: 'all',
  });

  // Emulation Cores State
  const [cores, setCores] = useState<EmulatorCore[]>(INITIAL_CORES);
  const [coreDownloadState, setCoreDownloadState] = useState<CoreDownloadState>({
    isDownloading: false,
    coreId: null,
    progress: 0,
    currentCoreName: '',
    speed: '0 MB/s',
    downloadedBytes: '',
  });
  const coreDownloadTimerRef = useRef<NodeJS.Timeout | null>(null);

  // Navigation with sound wrapper
  const setCurrentView = (view: ViewMode) => {
    soundManager.playSelect();
    setCurrentViewRaw(view);
  };

  const setSelectedConsole = (id: ConsoleId) => {
    soundManager.playNavTick();
    setSelectedConsoleRaw(id);
  };

  const setSelectedGame = (game: RomGame | null) => {
    if (game) soundManager.playSelect();
    else soundManager.playBack();
    setSelectedGameRaw(game);
  };

  const setRunningGame = (game: RomGame | null) => {
    if (game) {
      soundManager.playLaunchSound();
      setGames(prev =>
        prev.map(g =>
          g.id === game.id ? { ...g, launchCount: (g.launchCount || 0) + 1 } : g
        )
      );
    } else {
      soundManager.playBack();
    }
    setRunningGameRaw(game);
  };

  // Toggle favorite
  const toggleFavorite = (gameId: string) => {
    soundManager.playFavorite();
    setGames(prev =>
      prev.map(g => (g.id === gameId ? { ...g, isFavorite: !g.isFavorite } : g))
    );
    if (selectedGame && selectedGame.id === gameId) {
      setSelectedGameRaw(prev => prev ? { ...prev, isFavorite: !prev.isFavorite } : null);
    }
  };

  // Theme actions
  const activeAccent = ACCENT_COLORS.find(c => c.id === theme.accentId) || ACCENT_COLORS[0];

  const setAccentColor = (accentId: string) => {
    soundManager.playSelect();
    setTheme(prev => ({ ...prev, accentId }));
  };

  const setGlassOpacity = (glassOpacity: number) => {
    setTheme(prev => ({ ...prev, glassOpacity }));
  };

  const setGlassBlur = (glassBlur: number) => {
    setTheme(prev => ({ ...prev, glassBlur }));
  };

  const setColorMode = (colorMode: 'dark' | 'light') => {
    soundManager.playNavTick();
    setTheme(prev => ({ ...prev, colorMode }));
  };

  const setWallpaper = (wallpaperId: string) => {
    soundManager.playSelect();
    setTheme(prev => ({ ...prev, wallpaperId }));
  };

  const setSoundEnabled = (enabled: boolean) => {
    soundManager.setEnabled(enabled);
    if (enabled) soundManager.playSelect();
    setTheme(prev => ({ ...prev, soundEnabled: enabled }));
  };

  const setCrtEffect = (enabled: boolean) => {
    soundManager.playNavTick();
    setTheme(prev => ({ ...prev, crtEffect: enabled }));
  };

  // Sync CSS custom variables with Theme (No Neon Bloom, Frosted Glass)
  useEffect(() => {
    const root = document.documentElement;
    root.style.setProperty('--accent-color', activeAccent.hex);
    root.style.setProperty('--accent-border', activeAccent.border);
    root.style.setProperty('--glass-blur', `${theme.glassBlur}px`);
    root.style.setProperty('--glass-bg', theme.colorMode === 'dark' 
      ? `rgba(15, 23, 42, ${theme.glassOpacity})` 
      : `rgba(255, 255, 255, ${theme.glassOpacity + 0.15})`);
    root.style.setProperty('--glass-card-bg', theme.colorMode === 'dark' 
      ? `rgba(255, 255, 255, ${theme.glassOpacity * 0.4})` 
      : `rgba(255, 255, 255, ${theme.glassOpacity * 0.95})`);
    root.style.setProperty('--glass-card-hover', theme.colorMode === 'dark' 
      ? `rgba(255, 255, 255, ${theme.glassOpacity * 0.8})` 
      : `rgba(255, 255, 255, 0.45)`);
    root.style.setProperty('--glass-border', theme.colorMode === 'dark'
      ? 'rgba(255, 255, 255, 0.12)'
      : 'rgba(0, 0, 0, 0.1)');

    if (theme.colorMode === 'dark') {
      root.classList.add('dark');
    } else {
      root.classList.remove('dark');
    }
  }, [theme, activeAccent]);

  // Controller Preset
  const setPreset = (presetId: ControllerPresetId) => {
    soundManager.playSelect();
    const preset = CONTROLLER_PRESETS[presetId];
    if (preset) {
      setControllerMapping({
        preset: presetId,
        bindings: { ...preset.bindings },
      });
    }
  };

  const updateBinding = (buttonId: string, key: string) => {
    soundManager.playSelect();
    setControllerMapping(prev => ({
      ...prev,
      bindings: {
        ...prev.bindings,
        [buttonId]: key,
      },
    }));
  };

  // ROM Scanner Simulation (Embedded, non-modifiable directories)
  const cancelScan = useCallback(() => {
    setScanState(prev => ({ ...prev, isScanning: false }));
    soundManager.playBack();
  }, []);

  const startScan = useCallback((targetConsole: ConsoleId = selectedConsole) => {
    soundManager.playSelect();
    setShowScanModal(true);

    const consoleObj = CONSOLES.find(c => c.id === targetConsole);
    const directory = targetConsole === 'all' 
      ? '/sdcard/roms/' 
      : consoleObj?.romDirectory || `/sdcard/roms/${targetConsole}/`;

    setScanState(prev => ({
      ...prev,
      isScanning: true,
      progress: 0,
      currentTarget: directory,
      currentFile: `Verificando archivos en ${directory}...`,
      foundCount: 0,
      selectedConsoleId: targetConsole,
      logs: [
        {
          id: Date.now().toString(),
          text: targetConsole === 'all'
            ? 'Iniciando escaneo completo en todo /sdcard/roms/'
            : `Iniciando escaneo de directorio fijo: ${directory} (${consoleObj?.fullName})`,
          type: 'info',
          time: new Date().toLocaleTimeString(),
        },
      ],
    }));

    const sampleFiles = [
      { name: 'Donkey Kong Country (USA).sfc', console: 'snes' as ConsoleId },
      { name: 'Gran Turismo 2 (Disc 1).chd', console: 'ps1' as ConsoleId },
      { name: 'Golden Sun (USA).gba', console: 'gba' as ConsoleId },
      { name: 'Super Mario Bros 3 (USA).nes', console: 'nes' as ConsoleId },
      { name: 'Gunstar Heroes (USA).md', console: 'genesis' as ConsoleId },
      { name: 'Banjo-Kazooie (USA).z64', console: 'n64' as ConsoleId },
    ].filter(f => targetConsole === 'all' || f.console === targetConsole);

    let currentStep = 0;
    const interval = setInterval(() => {
      currentStep += 1;
      const progress = Math.min(100, Math.round((currentStep / 10) * 100));

      if (currentStep % 2 === 0) {
        const fileIdx = (Math.floor(currentStep / 2) - 1) % (sampleFiles.length || 1);
        const candidate = sampleFiles[fileIdx] || { 
          name: `ROM_Archive_${currentStep}.${targetConsole === 'ps1' ? 'chd' : 'zip'}`, 
          console: targetConsole 
        };
        
        soundManager.playScanBlip();

        setScanState(prev => {
          const logEntry: ScanLog = {
            id: `${Date.now()}-${currentStep}`,
            text: `Indexado: [${candidate.console.toUpperCase()}] ${candidate.name} (CRC32 OK)`,
            type: 'success',
            time: new Date().toLocaleTimeString(),
          };
          return {
            ...prev,
            progress,
            currentFile: candidate.name,
            foundCount: prev.foundCount + 1,
            logs: [logEntry, ...prev.logs.slice(0, 14)],
          };
        });
      } else {
        setScanState(prev => ({
          ...prev,
          progress,
          currentFile: `Leyendo cabeceras en ${directory}sector ${currentStep * 328}...`,
        }));
      }

      if (currentStep >= 10) {
        clearInterval(interval);
        soundManager.playSelect();
        setScanState(prev => ({
          ...prev,
          isScanning: false,
          progress: 100,
          currentFile: 'Escaneo concluido.',
          logs: [
            {
              id: Date.now().toString(),
              text: `Completado: Directorio ${directory} sincronizado. ${prev.foundCount} ROMs verificadas.`,
              type: 'info',
              time: new Date().toLocaleTimeString(),
            },
            ...prev.logs,
          ],
        }));
      }
    }, 400);
  }, [selectedConsole]);

  // Core Download Handlers
  const downloadCore = useCallback((coreId: string) => {
    const targetCore = cores.find(c => c.id === coreId);
    if (!targetCore) return;

    soundManager.playSelect();
    setCoreDownloadState({
      isDownloading: true,
      coreId,
      progress: 8,
      currentCoreName: targetCore.name,
      speed: '5.2 MB/s',
      downloadedBytes: `0.3 / ${targetCore.fileSize}`,
    });

    let currentProgress = 8;
    if (coreDownloadTimerRef.current) clearInterval(coreDownloadTimerRef.current);

    coreDownloadTimerRef.current = setInterval(() => {
      currentProgress += Math.floor(Math.random() * 16) + 12;
      if (currentProgress >= 100) {
        clearInterval(coreDownloadTimerRef.current!);
        setCoreDownloadState({
          isDownloading: false,
          coreId: null,
          progress: 100,
          currentCoreName: '',
          speed: '0 MB/s',
          downloadedBytes: '',
        });
        setCores(prev => prev.map(c => c.id === coreId ? { ...c, isInstalled: true, updateAvailable: false } : c));
        soundManager.playFavorite();
      } else {
        const speedFloat = (4.8 + Math.random() * 2).toFixed(1);
        const approxBytes = ((parseFloat(targetCore.fileSize) * currentProgress) / 100).toFixed(1);
        setCoreDownloadState(prev => ({
          ...prev,
          progress: currentProgress,
          speed: `${speedFloat} MB/s`,
          downloadedBytes: `${approxBytes} / ${targetCore.fileSize}`,
        }));
      }
    }, 200);
  }, [cores]);

  const downloadAllCores = useCallback(() => {
    const pendingCores = cores.filter(c => !c.isInstalled || c.updateAvailable);
    if (pendingCores.length === 0) return;

    soundManager.playSelect();
    let index = 0;
    let stepProgress = 0;

    const startDownloadItem = () => {
      if (index >= pendingCores.length) {
        setCoreDownloadState({
          isDownloading: false,
          coreId: null,
          progress: 100,
          currentCoreName: '',
          speed: '0 MB/s',
          downloadedBytes: '',
        });
        setCores(prev => prev.map(c => ({ ...c, isInstalled: true, updateAvailable: false })));
        soundManager.playFavorite();
        return;
      }

      const core = pendingCores[index];
      stepProgress = 0;

      setCoreDownloadState({
        isDownloading: true,
        coreId: 'all',
        progress: Math.round((index / pendingCores.length) * 100),
        currentCoreName: `${core.name} (${index + 1}/${pendingCores.length})`,
        speed: '5.8 MB/s',
        downloadedBytes: `0 / ${core.fileSize}`,
      });

      if (coreDownloadTimerRef.current) clearInterval(coreDownloadTimerRef.current);

      coreDownloadTimerRef.current = setInterval(() => {
        stepProgress += 25;
        if (stepProgress >= 100) {
          clearInterval(coreDownloadTimerRef.current!);
          setCores(prev => prev.map(c => c.id === core.id ? { ...c, isInstalled: true, updateAvailable: false } : c));
          index++;
          startDownloadItem();
        } else {
          const overall = Math.round(((index + stepProgress / 100) / pendingCores.length) * 100);
          const approxBytes = ((parseFloat(core.fileSize) * stepProgress) / 100).toFixed(1);
          setCoreDownloadState(prev => ({
            ...prev,
            progress: overall,
            speed: `${(4.8 + Math.random() * 1.5).toFixed(1)} MB/s`,
            downloadedBytes: `${approxBytes} / ${core.fileSize}`,
          }));
        }
      }, 150);
    };

    startDownloadItem();
  }, [cores]);

  const deleteCore = useCallback((coreId: string) => {
    soundManager.playBack();
    setCores(prev => prev.map(c => c.id === coreId ? { ...c, isInstalled: false } : c));
  }, []);

  const cancelCoreDownload = useCallback(() => {
    if (coreDownloadTimerRef.current) clearInterval(coreDownloadTimerRef.current);
    setCoreDownloadState({
      isDownloading: false,
      coreId: null,
      progress: 0,
      currentCoreName: '',
      speed: '0 MB/s',
      downloadedBytes: '',
    });
    soundManager.playBack();
  }, []);

  return (
    <RetroContext.Provider
      value={{
        currentView,
        setCurrentView,
        settingsTab,
        setSettingsTab,
        selectedConsole,
        setSelectedConsole,
        searchQuery,
        setSearchQuery,
        games,
        hoveredGame,
        setHoveredGame,
        focusedGame,
        setFocusedGame,
        selectedGame,
        setSelectedGame,
        runningGame,
        setRunningGame,
        toggleFavorite,
        focusedIndex,
        setFocusedIndex,
        theme,
        activeAccent,
        setAccentColor,
        setGlassOpacity,
        setGlassBlur,
        setColorMode,
        setWallpaper,
        setSoundEnabled,
        setCrtEffect,
        scanState,
        startScan,
        cancelScan,
        showScanModal,
        setShowScanModal,
        controllerMapping,
        setPreset,
        updateBinding,
        cores,
        coreDownloadState,
        downloadCore,
        downloadAllCores,
        deleteCore,
        cancelCoreDownload,
      }}
    >
      {children}
    </RetroContext.Provider>
  );
};

export const useRetro = () => {
  const context = useContext(RetroContext);
  if (!context) {
    throw new Error('useRetro must be used within a RetroProvider');
  }
  return context;
};
