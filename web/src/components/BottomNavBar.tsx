import React, { useState, useEffect } from 'react';
import { 
  Gamepad2, 
  Layers, 
  SlidersHorizontal, 
  HardDrive,
  Star,
  Play,
  Info
} from 'lucide-react';
import { useRetro, ViewMode } from '../context/RetroContext';
import { soundManager } from '../utils/audio';

export const BottomNavBar: React.FC = () => {
  const { 
    currentView, 
    setCurrentView, 
    activeAccent, 
    games,
    hoveredGame,
    focusedGame,
    toggleFavorite,
    setRunningGame,
    setSelectedGame
  } = useRetro();

  const [timeStr, setTimeStr] = useState<string>('');

  useEffect(() => {
    const updateTime = () => {
      const now = new Date();
      setTimeStr(
        now.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })
      );
    };
    updateTime();
    const interval = setInterval(updateTime, 1000);
    return () => clearInterval(interval);
  }, []);

  const navItems: Array<{ id: ViewMode; label: string; icon: React.FC<{ className?: string }> }> = [
    { id: 'home', label: 'Inicio', icon: Gamepad2 },
    { id: 'consoles', label: 'Consolas', icon: Layers },
    { id: 'settings', label: 'Ajustes', icon: SlidersHorizontal },
  ];

  // Active game is the gamepad-focused game in Home/Consoles
  const activeGame = hoveredGame || ((currentView === 'home' || currentView === 'consoles') ? focusedGame : null);

  return (
    <nav className="relative z-30 flex items-center justify-between gap-4 px-6 py-2.5 border-t border-white/15 glass-panel bg-slate-950/75 backdrop-blur-2xl select-none min-h-[58px]">
      {/* Zone 1 (LEFT): Informative Game Action Buttons (Favorito, Detalle, Lanzar) */}
      <div className="flex items-center gap-2 z-10 shrink-0">
        {activeGame ? (
          <div className="flex items-center gap-2 animate-in fade-in slide-in-from-left-3 duration-200">
            {/* Favorito Button */}
            <button
              onClick={() => {
                soundManager.playNavTick();
                toggleFavorite(activeGame.id);
              }}
              title={activeGame.isFavorite ? 'Quitar de favoritos (Y)' : 'Marcar como favorito (Y)'}
              className={`flex items-center gap-1.5 px-3 py-1.5 rounded-xl text-xs font-medium transition-all cursor-pointer ${
                activeGame.isFavorite
                  ? 'bg-amber-500/25 border border-amber-400 text-amber-300 shadow-md backdrop-blur-md'
                  : 'glass-button text-slate-300 hover:text-white'
              }`}
            >
              <Star className={`w-3.5 h-3.5 ${activeGame.isFavorite ? 'fill-current' : ''}`} />
              <span className="hidden sm:inline">
                {activeGame.isFavorite ? 'Favorito' : 'Favorito'}
              </span>
              <span className="text-[10px] font-mono px-1 py-0.2 rounded bg-white/10 text-slate-300">
                Y
              </span>
            </button>

            {/* Detalle Button (X) */}
            <button
              onClick={() => {
                soundManager.playNavTick();
                setSelectedGame(activeGame);
              }}
              title="Ver detalle del juego (X)"
              className="flex items-center gap-1.5 px-3 py-1.5 rounded-xl text-xs font-semibold glass-button text-slate-200 hover:text-white transition-all cursor-pointer"
            >
              <Info className="w-3.5 h-3.5 text-sky-400" />
              <span>Detalle</span>
              <span className="text-[10px] font-mono font-bold px-1.5 py-0.5 rounded bg-white/15 text-white">
                X
              </span>
            </button>

            {/* Lanzar Button (A) */}
            <button
              onClick={() => {
                soundManager.playLaunch();
                setRunningGame(activeGame);
              }}
              title="Lanzar juego directamente (A)"
              className="flex items-center gap-2 px-4 py-1.5 rounded-xl text-xs font-bold text-slate-900 bg-white hover:bg-slate-100 shadow-xl transition-all transform active:scale-95 cursor-pointer"
              style={{
                boxShadow: `0 4px 14px ${activeAccent.hex}40`,
              }}
            >
              <Play className="w-3.5 h-3.5 fill-current" />
              <span>Lanzar</span>
              <span className="text-[10px] font-mono font-bold px-1.5 py-0.5 rounded bg-black/30 text-slate-900">
                A
              </span>
            </button>
          </div>
        ) : (
          <div className="h-8" />
        )}
      </div>

      {/* Zone 2 (CENTER): Centered Navigation Links (Inicio, Consolas, Ajustes) */}
      <div className="absolute left-1/2 -translate-x-1/2 flex items-center gap-1.5 p-1 rounded-2xl bg-slate-950/60 border border-white/15 backdrop-blur-xl shadow-inner z-20">
        {navItems.map(item => {
          const Icon = item.icon;
          const isActive = currentView === item.id;
          return (
            <button
              key={item.id}
              onClick={() => {
                soundManager.playNavTick();
                setCurrentView(item.id);
              }}
              className={`flex items-center gap-2 px-4 py-1.5 text-xs rounded-xl whitespace-nowrap shrink-0 transition-all cursor-pointer ${
                isActive
                  ? 'text-white shadow-md font-semibold bg-white/15 border border-white/30'
                  : 'text-slate-300 hover:text-white hover:bg-white/5 font-medium'
              }`}
              style={
                isActive
                  ? {
                      backgroundColor: 'rgba(255, 255, 255, 0.14)',
                      borderColor: 'rgba(255, 255, 255, 0.3)',
                      boxShadow: 'inset 0 1px 1px rgba(255, 255, 255, 0.25)',
                    }
                  : {}
              }
            >
              <Icon className="w-3.5 h-3.5" />
              <span>{item.label}</span>
            </button>
          );
        })}
      </div>

      {/* Zone 3 (RIGHT): System Indicators (ROMs Count + Live Clock) */}
      <div className="flex items-center gap-2.5 shrink-0 z-10">
        {/* SD Card ROM count */}
        <div className="flex items-center gap-1.5 px-3 py-1.5 rounded-xl glass-pill text-xs text-slate-300">
          <HardDrive className="w-3.5 h-3.5 text-slate-400" />
          <span className="font-mono text-[11px] tabular-nums font-medium">
            {games.length} ROMs
          </span>
        </div>

        {/* Live Clock */}
        <div className="px-3.5 py-1.5 rounded-xl glass-pill font-mono text-xs font-semibold text-slate-200 tabular-nums">
          {timeStr || '12:00'}
        </div>
      </div>
    </nav>
  );
};
