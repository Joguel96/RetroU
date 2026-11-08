export type ConsoleId = 
  | 'all'
  | 'snes' 
  | 'ps1' 
  | 'gba' 
  | 'nes' 
  | 'genesis' 
  | 'n64' 
  | 'arcade' 
  | 'psp';

export interface ConsoleInfo {
  id: ConsoleId;
  name: string;
  fullName: string;
  manufacturer: string;
  year: number;
  color: string;
  accentHex: string;
  core: string;
  romDirectory: string;
  romExtensions: string[];
  totalRoms: number;
  iconName: string;
  tagline: string;
}

export interface RomGame {
  id: string;
  title: string;
  consoleId: ConsoleId;
  releaseYear: number;
  developer: string;
  publisher: string;
  genre: string;
  rating: number; // 0 - 5
  playTimeMinutes: number;
  launchCount: number; // Veces que se manda a ejecutar
  lastPlayed?: string;
  isFavorite: boolean;
  coverGradient: [string, string];
  coverIcon: string;
  description: string;
  fileSize: string;
  filename: string;
  featured?: boolean;
  // PS3 / PS4 style dynamic wallpaper backdrop properties
  backdropGradient: [string, string, string];
  backdropTheme: string;
  backdropQuote: string;
}

export type AccentColor = {
  id: string;
  name: string;
  hex: string;
  subtle: string;
  border: string;
  badgeBg: string;
};

export type WallpaperOption = {
  id: string;
  name: string;
  type: 'gradient' | 'minimal' | 'aurora' | 'nebula' | 'carbon' | 'slate';
  description: string;
  previewGradient: string;
};

export interface ThemeSettings {
  accentId: string;
  glassOpacity: number; // 0.05 to 0.40
  glassBlur: number; // 4 to 32px
  colorMode: 'dark' | 'light';
  wallpaperId: string;
  soundEnabled: boolean;
  crtEffect: boolean;
}

export type ControllerPresetId = 'wiiu' | 'switch-pro' | 'playstation' | 'xbox' | 'retro-8bit';

export interface ControllerMapping {
  preset: ControllerPresetId;
  bindings: Record<string, string>;
}

export interface ScanLog {
  id: string;
  text: string;
  type: 'info' | 'success' | 'warn';
  time: string;
}

export interface ScanState {
  isScanning: boolean;
  progress: number;
  currentTarget: string;
  currentFile: string;
  foundCount: number;
  logs: ScanLog[];
  selectedConsoleId: ConsoleId;
}

export interface EmulatorCore {
  id: string;
  name: string;
  systemName: string;
  consoleId: ConsoleId;
  version: string;
  fileSize: string;
  isInstalled: boolean;
  description: string;
  author: string;
  license: string;
  updateAvailable?: boolean;
}

export interface CoreDownloadState {
  isDownloading: boolean;
  coreId: string | 'all' | null;
  progress: number;
  currentCoreName: string;
  speed: string;
  downloadedBytes: string;
}
