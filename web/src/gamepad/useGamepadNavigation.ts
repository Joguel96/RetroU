import { useEffect, useRef, useState } from 'react';
import { useRetro } from '../context/RetroContext';

type Direction = 'up' | 'down' | 'left' | 'right';

const DEADZONE = 0.55;
const FIRST_REPEAT_MS = 320;
const REPEAT_MS = 140;
const FOCUS_CLASS = 'gamepad-focused';

const FOCUSABLE_SELECTOR = [
  'button',
  '[role="button"]',
  'a[href]',
  'input',
  'select',
  '[data-gamepad-focusable]',
].join(',');

function isVisible(element: HTMLElement): boolean {
  if (element.hasAttribute('disabled') || element.getAttribute('aria-hidden') === 'true') {
    return false;
  }
  const style = window.getComputedStyle(element);
  const rect = element.getBoundingClientRect();
  return style.display !== 'none' && style.visibility !== 'hidden' && rect.width > 2 && rect.height > 2;
}

function getFocusables(): HTMLElement[] {
  return Array.from(document.querySelectorAll<HTMLElement>(FOCUSABLE_SELECTOR)).filter(
    (element) => !element.closest('[data-gamepad-ignore]') && isVisible(element),
  );
}

function dispatchSyntheticKey(key: string): void {
  window.dispatchEvent(
    new KeyboardEvent('keydown', {
      key,
      bubbles: true,
      cancelable: true,
    }),
  );
}

function setRangeValue(input: HTMLInputElement, direction: Direction): void {
  const step = Number(input.step) || 1;
  const min = Number(input.min) || 0;
  const max = Number(input.max) || 100;
  const current = Number(input.value) || 0;
  const next = Math.min(max, Math.max(min, current + (direction === 'right' || direction === 'up' ? step : -step)));
  const setter = Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, 'value')?.set;
  setter?.call(input, String(next));
  input.dispatchEvent(new Event('input', { bubbles: true }));
  input.dispatchEvent(new Event('change', { bubbles: true }));
}

function changeSelect(select: HTMLSelectElement, direction: Direction): void {
  if (select.options.length === 0) return;
  const delta = direction === 'down' || direction === 'right' ? 1 : -1;
  const next = Math.min(select.options.length - 1, Math.max(0, select.selectedIndex + delta));
  select.selectedIndex = next;
  select.dispatchEvent(new Event('change', { bubbles: true }));
}

export function useGamepadNavigation(): { connected: boolean } {
  const { games, setFocusedGame, setCurrentView, currentView } = useRetro();
  const [connected, setConnected] = useState(false);
  const focusedRef = useRef<HTMLElement | null>(null);
  const stateRef = useRef({
    games,
    setFocusedGame,
    setCurrentView,
    currentView,
  });

  stateRef.current = { games, setFocusedGame, setCurrentView, currentView };

  useEffect(() => {
    let frame = 0;
    let lastButtons: boolean[] = [];
    let lastDirection: Direction | null = null;
    let nextRepeatAt = 0;

    const clearFocus = () => {
      focusedRef.current?.classList.remove(FOCUS_CLASS);
      focusedRef.current = null;
    };

    const focusElement = (element: HTMLElement) => {
      if (focusedRef.current === element) return;
      focusedRef.current?.classList.remove(FOCUS_CLASS);
      focusedRef.current = element;
      element.classList.add(FOCUS_CLASS);
      element.scrollIntoView({ block: 'nearest', inline: 'nearest', behavior: 'smooth' });

      const card = element.closest<HTMLElement>('[data-game-id]');
      const gameId = card?.dataset.gameId;
      if (gameId) {
        const game = stateRef.current.games.find((item) => item.id === gameId);
        if (game) stateRef.current.setFocusedGame(game);
      }
    };

    const ensureFocus = () => {
      if (focusedRef.current && focusedRef.current.isConnected && isVisible(focusedRef.current)) {
        return focusedRef.current;
      }
      const first =
        document.querySelector<HTMLElement>('[data-game-id]') ||
        getFocusables()[0];
      if (first) focusElement(first);
      return focusedRef.current;
    };

    const moveFocus = (direction: Direction) => {
      const current = ensureFocus();
      if (!current) return;

      if (current instanceof HTMLInputElement && current.type === 'range') {
        setRangeValue(current, direction);
        return;
      }
      if (current instanceof HTMLSelectElement) {
        changeSelect(current, direction);
        return;
      }

      const from = current.getBoundingClientRect();
      const cx = from.left + from.width / 2;
      const cy = from.top + from.height / 2;
      const candidates = getFocusables().filter((element) => element !== current);

      let best: HTMLElement | null = null;
      let bestScore = Number.POSITIVE_INFINITY;

      for (const candidate of candidates) {
        const rect = candidate.getBoundingClientRect();
        const x = rect.left + rect.width / 2;
        const y = rect.top + rect.height / 2;
        const dx = x - cx;
        const dy = y - cy;

        let primary = 0;
        let secondary = 0;
        if (direction === 'up') {
          if (dy > -8) continue;
          primary = -dy;
          secondary = Math.abs(dx);
        } else if (direction === 'down') {
          if (dy < 8) continue;
          primary = dy;
          secondary = Math.abs(dx);
        } else if (direction === 'left') {
          if (dx > -8) continue;
          primary = -dx;
          secondary = Math.abs(dy);
        } else {
          if (dx < 8) continue;
          primary = dx;
          secondary = Math.abs(dy);
        }

        const score = primary + secondary * 0.55;
        if (score < bestScore) {
          bestScore = score;
          best = candidate;
        }
      }

      if (best) focusElement(best);
    };

    const activate = () => {
      const current = ensureFocus();
      if (!current) return;

      if (current instanceof HTMLInputElement) {
        if (current.type === 'range') return;
        current.focus();
        return;
      }
      if (current instanceof HTMLSelectElement) {
        current.focus();
        return;
      }
      current.click();
    };

    const cycleView = (delta: number) => {
      const order = ['home', 'consoles', 'settings'] as const;
      const index = order.indexOf(stateRef.current.currentView as (typeof order)[number]);
      stateRef.current.setCurrentView(order[(index + delta + order.length) % order.length]);
      clearFocus();
    };

    const poll = (time: number) => {
      const pads = navigator.getGamepads ? navigator.getGamepads() : [];
      const pad = Array.from(pads).find((item) => item && item.connected);

      if (!pad) {
        setConnected(false);
        lastButtons = [];
        lastDirection = null;
        frame = requestAnimationFrame(poll);
        return;
      }

      setConnected(true);

      const pressed = (index: number) => Boolean(pad.buttons[index]?.pressed);
      const edge = (index: number) => pressed(index) && !lastButtons[index];

      if (edge(0)) activate();
      if (edge(1)) dispatchSyntheticKey('Escape');
      if (edge(2)) dispatchSyntheticKey('x');
      if (edge(3)) dispatchSyntheticKey('f');
      if (edge(4)) cycleView(-1);
      if (edge(5)) cycleView(1);
      if (edge(8)) dispatchSyntheticKey('Escape');
      if (edge(9)) dispatchSyntheticKey('h');

      let direction: Direction | null = null;
      if (pressed(12)) direction = 'up';
      else if (pressed(13)) direction = 'down';
      else if (pressed(14)) direction = 'left';
      else if (pressed(15)) direction = 'right';
      else {
        const x = pad.axes[0] ?? 0;
        const y = pad.axes[1] ?? 0;
        if (Math.abs(x) >= DEADZONE || Math.abs(y) >= DEADZONE) {
          direction = Math.abs(x) > Math.abs(y) ? (x > 0 ? 'right' : 'left') : y > 0 ? 'down' : 'up';
        }
      }

      if (direction) {
        if (direction !== lastDirection) {
          moveFocus(direction);
          nextRepeatAt = time + FIRST_REPEAT_MS;
        } else if (time >= nextRepeatAt) {
          moveFocus(direction);
          nextRepeatAt = time + REPEAT_MS;
        }
      } else {
        nextRepeatAt = 0;
      }

      lastDirection = direction;
      lastButtons = pad.buttons.map((button) => button.pressed);
      frame = requestAnimationFrame(poll);
    };

    const onConnect = () => setConnected(true);
    const onDisconnect = () => setConnected(false);
    window.addEventListener('gamepadconnected', onConnect);
    window.addEventListener('gamepaddisconnected', onDisconnect);
    frame = requestAnimationFrame(poll);

    return () => {
      cancelAnimationFrame(frame);
      window.removeEventListener('gamepadconnected', onConnect);
      window.removeEventListener('gamepaddisconnected', onDisconnect);
      clearFocus();
    };
  }, []);

  return { connected };
}
