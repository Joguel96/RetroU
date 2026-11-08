import React, { useState, useEffect, useRef } from 'react';
import { 
  X, 
  Maximize2, 
  Minimize2, 
  Tv, 
  Camera, 
  Save, 
  Download, 
  Volume2, 
  VolumeX, 
  Play, 
  Pause,
  RotateCcw
} from 'lucide-react';
import { RomGame } from '../types';
import { useRetro } from '../context/RetroContext';
import { soundManager } from '../utils/audio';

interface EmulatorModalProps {
  game: RomGame;
  onClose: () => void;
}

export const EmulatorModal: React.FC<EmulatorModalProps> = ({ game, onClose }) => {
  const { activeAccent, theme } = useRetro();
  const [isPaused, setIsPaused] = useState(false);
  const [crtEnabled, setCrtEnabled] = useState(true);
  const [audioMuted, setAudioMuted] = useState(false);
  const [saveNotification, setSaveNotification] = useState<string | null>(null);
  const [fps, setFps] = useState(60);
  const canvasRef = useRef<HTMLCanvasElement | null>(null);

  // Mini canvas retro interactive animation loop
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    let animId: number;
    let playerX = 160;
    let playerY = 180;
    let angle = 0;
    const stars: Array<{ x: number; y: number; speed: number; size: number }> = [];

    // Initialize retro starfield
    for (let i = 0; i < 60; i++) {
      stars.push({
        x: Math.random() * 320,
        y: Math.random() * 240,
        speed: 0.5 + Math.random() * 2,
        size: Math.random() > 0.8 ? 2 : 1,
      });
    }

    const render = () => {
      if (!isPaused) {
        // Clear background with retro deep black/purple
        ctx.fillStyle = '#060814';
        ctx.fillRect(0, 0, 320, 240);

        // Draw starfield
        ctx.fillStyle = '#ffffff';
        stars.forEach(s => {
          s.y += s.speed;
          if (s.y > 240) {
            s.y = 0;
            s.x = Math.random() * 320;
          }
          ctx.fillRect(Math.floor(s.x), Math.floor(s.y), s.size, s.size);
        });

        // Floating retro grid floor
        ctx.strokeStyle = 'rgba(255, 255, 255, 0.15)';
        ctx.lineWidth = 1;
        ctx.beginPath();
        for (let i = 0; i < 320; i += 32) {
          ctx.moveTo(i, 160);
          ctx.lineTo(i * 1.5 - 80, 240);
        }
        for (let j = 160; j < 240; j += 16) {
          ctx.moveTo(0, j);
          ctx.lineTo(320, j);
        }
        ctx.stroke();

        // Player / Sprite
        angle += 0.04;
        playerY = 175 + Math.sin(angle) * 12;
        playerX = 160 + Math.cos(angle * 0.7) * 45;

        // Draw glowing ship/avatar
        ctx.save();
        ctx.translate(playerX, playerY);

        // Core icon representation
        ctx.fillStyle = activeAccent.hex;
        ctx.shadowColor = activeAccent.hex;
        ctx.shadowBlur = 12;

        // Triangle starship
        ctx.beginPath();
        ctx.moveTo(0, -14);
        ctx.lineTo(12, 10);
        ctx.lineTo(0, 5);
        ctx.lineTo(-12, 10);
        ctx.closePath();
        ctx.fill();

        // Engine glow
        ctx.fillStyle = '#f59e0b';
        ctx.beginPath();
        ctx.arc(0, 8, 3 + Math.random() * 2, 0, Math.PI * 2);
        ctx.fill();

        ctx.restore();

        // HUD overlay in canvas
        ctx.font = '8px monospace';
        ctx.fillStyle = 'rgba(255, 255, 255, 0.7)';
        ctx.fillText(`CORE: ${game.consoleId.toUpperCase()}_EMU 60.0 FPS`, 10, 16);
        ctx.fillText(`ROM: ${game.filename.slice(0, 24)}`, 10, 26);
        ctx.fillText(`STATE 0: READY`, 10, 36);
      }

      animId = requestAnimationFrame(render);
    };

    render();

    return () => cancelAnimationFrame(animId);
  }, [isPaused, activeAccent, game]);

  const triggerSaveState = () => {
    soundManager.playSelect();
    setSaveNotification('Estado 1 Guardado en Flash NAND');
    setTimeout(() => setSaveNotification(null), 2500);
  };

  const triggerLoadState = () => {
    soundManager.playSelect();
    setSaveNotification('Estado 1 Cargado con éxito');
    setTimeout(() => setSaveNotification(null), 2500);
  };

  const triggerScreenshot = () => {
    soundManager.playFavorite();
    setSaveNotification('Captura guardada en /sdcard/screenshots/');
    setTimeout(() => setSaveNotification(null), 2500);
  };

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center p-2 sm:p-6 bg-black/85 backdrop-blur-xl animate-in fade-in duration-200">
      <div 
        className="relative w-full max-w-4xl rounded-3xl overflow-hidden border border-white/20 glass-panel shadow-2xl flex flex-col"
      >
        {/* Top Control Header */}
        <div className="flex items-center justify-between px-6 py-3 border-b border-white/15 bg-slate-950/50 backdrop-blur-md">
          <div className="flex items-center gap-3">
            <span 
              className="px-3 py-1 rounded-xl text-xs font-mono font-bold tracking-wider uppercase glass-pill"
              style={{
                color: activeAccent.hex,
                borderColor: 'rgba(255, 255, 255, 0.2)',
              }}
            >
              {game.consoleId.toUpperCase()}
            </span>
            <span className="text-sm font-bold text-white tracking-wide truncate max-w-xs">
              {game.title}
            </span>
            <span className="hidden sm:inline-flex items-center gap-1.5 text-xs font-mono text-emerald-400 glass-pill px-2.5 py-0.5">
              <span className="w-1.5 h-1.5 rounded-full bg-emerald-400 animate-pulse" />
              60.0 FPS
            </span>
          </div>

          <div className="flex items-center gap-2">
            {/* CRT Toggle */}
            <button
              onClick={() => setCrtEnabled(!crtEnabled)}
              className={`p-2 rounded-xl text-xs font-mono flex items-center gap-1 transition-all cursor-pointer ${
                crtEnabled 
                  ? 'bg-white/20 border border-white/30 text-white shadow-md' 
                  : 'glass-button text-slate-400'
              }`}
              title="Alternar filtro CRT Scanlines"
            >
              <Tv className="w-3.5 h-3.5" />
              <span className="hidden md:inline">CRT</span>
            </button>

            {/* Audio Toggle */}
            <button
              onClick={() => setAudioMuted(!audioMuted)}
              className="p-2 rounded-xl glass-button text-slate-300 hover:text-white cursor-pointer"
              title="Silenciar audio de emulación"
            >
              {audioMuted ? <VolumeX className="w-4 h-4 text-slate-500" /> : <Volume2 className="w-4 h-4 text-white" />}
            </button>

            {/* Exit */}
            <button
              onClick={onClose}
              className="p-2 rounded-xl bg-rose-500/20 hover:bg-rose-500/30 border border-rose-500/30 text-rose-300 transition-colors cursor-pointer"
              title="Salir del emulador (Esc / B)"
            >
              <X className="w-4 h-4" />
            </button>
          </div>
        </div>

        {/* Emulator Viewport Screen */}
        <div className="relative aspect-[4/3] max-h-[60vh] w-full bg-black flex items-center justify-center overflow-hidden">
          <canvas
            ref={canvasRef}
            width={320}
            height={240}
            className="w-full h-full object-contain image-rendering-pixelated"
          />

          {/* CRT Scanline Filter Overlay */}
          {crtEnabled && (
            <div className="crt-scanlines absolute inset-0 z-20 pointer-events-none opacity-50" />
          )}

          {/* CRT Vignette & Screen Curve */}
          <div className="absolute inset-0 z-10 pointer-events-none shadow-[inset_0_0_60px_rgba(0,0,0,0.85)]" />

          {/* Save/Load Notification Pill */}
          {saveNotification && (
            <div className="absolute top-4 left-1/2 -translate-x-1/2 z-30 px-4 py-1.5 rounded-full glass-panel border border-emerald-400/40 text-xs font-mono text-emerald-300 shadow-xl animate-bounce">
              {saveNotification}
            </div>
          )}

          {/* Pause Screen Overlay */}
          {isPaused && (
            <div className="absolute inset-0 z-30 flex items-center justify-center bg-black/60 backdrop-blur-sm">
              <div className="px-6 py-3 rounded-2xl glass-panel border border-white/20 text-center font-mono">
                <span className="text-xl font-bold tracking-widest text-amber-300">PAUSA</span>
                <p className="text-xs text-slate-400 mt-1">Presiona Reanudar para continuar</p>
              </div>
            </div>
          )}
        </div>

        {/* Bottom Control Bar */}
        <div className="px-6 py-3.5 border-t border-white/15 bg-slate-950/60 backdrop-blur-md flex flex-wrap items-center justify-between gap-4">
          <div className="flex items-center gap-2">
            <button
              onClick={() => setIsPaused(!isPaused)}
              className="flex items-center gap-1.5 px-3.5 py-1.5 rounded-xl glass-button text-xs font-semibold text-white cursor-pointer"
            >
              {isPaused ? <Play className="w-3.5 h-3.5 fill-current" /> : <Pause className="w-3.5 h-3.5" />}
              <span>{isPaused ? 'Reanudar' : 'Pausa'}</span>
            </button>

            <button
              onClick={triggerSaveState}
              className="flex items-center gap-1.5 px-3 py-1.5 rounded-xl glass-button text-xs text-slate-200 cursor-pointer"
            >
              <Save className="w-3.5 h-3.5 text-cyan-300" />
              <span>Guardar Estado</span>
            </button>

            <button
              onClick={triggerLoadState}
              className="flex items-center gap-1.5 px-3 py-1.5 rounded-xl glass-button text-xs text-slate-200 cursor-pointer"
            >
              <Download className="w-3.5 h-3.5 text-purple-300" />
              <span>Cargar Estado</span>
            </button>

            <button
              onClick={triggerScreenshot}
              className="p-2 rounded-xl glass-button text-xs text-slate-200 cursor-pointer"
              title="Tomar captura"
            >
              <Camera className="w-3.5 h-3.5" />
            </button>
          </div>

          <div className="flex items-center gap-2">
            <button
              onClick={onClose}
              className="flex items-center gap-1.5 px-4 py-2 rounded-xl font-bold text-xs text-slate-900 bg-white hover:bg-slate-100 shadow-md transition-all cursor-pointer"
              style={{
                boxShadow: 'inset 0 1px 1px rgba(255,255,255,0.4), 0 4px 12px rgba(0,0,0,0.4)',
              }}
            >
              <RotateCcw className="w-3 h-3" />
              <span>Volver a RetroU</span>
            </button>
          </div>
        </div>
      </div>
    </div>
  );
};
