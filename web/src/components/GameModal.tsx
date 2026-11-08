import React from 'react';
import { 
  X, 
  Play, 
  Star, 
  Clock, 
  HardDrive, 
  Cpu, 
  Calendar, 
  Building2, 
  Sparkles,
  Share2
} from 'lucide-react';
import { RomGame } from '../types';
import { useRetro } from '../context/RetroContext';
import { CONSOLES } from '../data/mockData';

interface GameModalProps {
  game: RomGame;
  onClose: () => void;
  onPlay: () => void;
}

export const GameModal: React.FC<GameModalProps> = ({ game, onClose, onPlay }) => {
  const { activeAccent, toggleFavorite } = useRetro();
  const consoleInfo = CONSOLES.find(c => c.id === game.consoleId);

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center p-4 sm:p-6 bg-black/70 backdrop-blur-md animate-in fade-in duration-200">
      <div 
        className="relative w-full max-w-3xl overflow-hidden rounded-3xl border border-white/20 glass-panel shadow-2xl"
        onClick={(e) => e.stopPropagation()}
      >
        {/* Modal Top Bar */}
        <div className="flex items-center justify-between px-6 py-4 border-b border-white/15 bg-slate-900/40 backdrop-blur-md">
          <div className="flex items-center gap-2.5">
            <span 
              className="px-3 py-1 rounded-xl text-xs font-mono font-bold tracking-wider uppercase glass-pill"
              style={{
                color: activeAccent.hex,
                borderColor: 'rgba(255, 255, 255, 0.2)',
              }}
            >
              {consoleInfo?.name || game.consoleId.toUpperCase()}
            </span>
            <span className="text-xs text-slate-300 font-mono">
              Core: {consoleInfo?.core.split(' ')[0]}
            </span>
          </div>

          <button
            onClick={onClose}
            className="p-2 rounded-xl glass-button text-slate-300 hover:text-white cursor-pointer"
          >
            <X className="w-4 h-4" />
          </button>
        </div>

        {/* Modal Content Body */}
        <div className="p-6 sm:p-8 grid grid-cols-1 md:grid-cols-12 gap-6 items-start max-h-[75vh] overflow-y-auto">
          {/* Cover Art Column */}
          <div className="md:col-span-5 flex flex-col items-center">
            <div 
              className="relative aspect-[4/3] w-full rounded-2xl overflow-hidden shadow-2xl flex items-center justify-center border border-white/20"
              style={{
                background: `linear-gradient(135deg, ${game.coverGradient[0]} 0%, ${game.coverGradient[1]} 100%)`,
                boxShadow: 'inset 0 1.5px 2px rgba(255,255,255,0.3), 0 20px 40px rgba(0,0,0,0.6)',
              }}
            >
              <div 
                className="absolute inset-0 opacity-20"
                style={{
                  backgroundImage: 'radial-gradient(circle at 1px 1px, rgba(255,255,255,0.4) 1px, transparent 0)',
                  backgroundSize: '16px 16px',
                }}
              />
              <span className="text-7xl drop-shadow-2xl">{game.coverIcon}</span>

              {game.featured && (
                <div className="absolute top-3 left-3 flex items-center gap-1.5 text-[11px] font-semibold text-slate-100 glass-pill px-3 py-1">
                  <Sparkles className="w-3 h-3 text-amber-300" />
                  <span>Joya Retro</span>
                </div>
              )}
            </div>

            {/* Quick Actions under cover */}
            <div className="w-full mt-4 flex items-center gap-2">
              <button
                onClick={() => toggleFavorite(game.id)}
                className={`flex-1 flex items-center justify-center gap-2 py-2.5 px-4 rounded-xl text-xs font-semibold transition-all cursor-pointer ${
                  game.isFavorite
                    ? 'bg-amber-500/25 border border-amber-400 text-amber-300 shadow-md backdrop-blur-md'
                    : 'glass-button text-slate-300'
                }`}
              >
                <Star className={`w-4 h-4 ${game.isFavorite ? 'fill-current' : ''}`} />
                <span>{game.isFavorite ? 'En Favoritos' : 'Añadir a Favoritos'}</span>
              </button>
            </div>
          </div>

          {/* Details Column */}
          <div className="md:col-span-7 flex flex-col justify-between h-full space-y-5">
            <div>
              <h2 className="text-2xl sm:text-3xl font-extrabold text-white tracking-tight">
                {game.title}
              </h2>

              <p className="mt-3 text-sm text-slate-300 leading-relaxed">
                {game.description}
              </p>
            </div>

            {/* Technical Metadata Grid */}
            <div className="grid grid-cols-2 gap-3 text-xs glass-card p-4 rounded-2xl border border-white/15 font-mono">
              <div className="flex items-center gap-2 text-slate-300">
                <Calendar className="w-4 h-4 text-slate-400" />
                <span>Lanzamiento: <strong className="text-white">{game.releaseYear}</strong></span>
              </div>
              <div className="flex items-center gap-2 text-slate-300">
                <Building2 className="w-4 h-4 text-slate-400" />
                <span className="truncate">Estudio: <strong className="text-white">{game.developer}</strong></span>
              </div>
              <div className="flex items-center gap-2 text-slate-300">
                <Clock className="w-4 h-4 text-slate-400" />
                <span>Ejecutado: <strong className="text-emerald-400">{game.launchCount || 0} veces</strong></span>
              </div>
              <div className="flex items-center gap-2 text-slate-300">
                <HardDrive className="w-4 h-4 text-slate-400" />
                <span>Archivo: <strong className="text-white">{game.fileSize}</strong></span>
              </div>
              <div className="col-span-2 flex items-center gap-2 text-slate-400 pt-2 border-t border-white/10">
                <Cpu className="w-4 h-4 text-slate-400" />
                <span className="truncate">ROM: <code className="text-slate-200">{game.filename}</code></span>
              </div>
            </div>

            {/* Primary Action Button Bar */}
            <div className="flex items-center gap-3 pt-2">
              <button
                onClick={() => {
                  onClose();
                  onPlay();
                }}
                className="flex-1 flex items-center justify-center gap-2.5 py-3.5 px-6 rounded-2xl font-bold text-sm text-slate-900 bg-white hover:bg-slate-100 shadow-xl transition-all transform active:scale-95 cursor-pointer"
                style={{
                  boxShadow: 'inset 0 1px 1px rgba(255,255,255,0.4), 0 10px 25px rgba(0,0,0,0.5)',
                }}
              >
                <Play className="w-4 h-4 fill-current" />
                <span>INICIAR EMULADOR (A)</span>
              </button>

              <button
                onClick={onClose}
                className="py-3.5 px-5 rounded-2xl glass-button text-sm font-semibold text-slate-300 hover:text-white cursor-pointer"
              >
                Cerrar (B)
              </button>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};
