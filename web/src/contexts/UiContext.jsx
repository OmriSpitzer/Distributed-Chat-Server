/**
 * UI context
 * @brief Light/dark and comfortable/compact for the browser dashboard.
 * @date 2026-09-30
 *
 * Holds the look and writes it onto the document. index.css paints from those attributes.
 */

import { createContext, useContext, useEffect, useState } from "react";

const UiContext = createContext(null);

export function UiProvider({ children }) {
  const [theme, setTheme] = useState("light");
  const [density, setDensity] = useState("comfortable");

  useEffect(() => {
    document.documentElement.dataset.theme = theme;
    document.documentElement.dataset.density = density;
  }, [theme, density]);

  function toggleTheme() {
    setTheme((current) => (current === "dark" ? "light" : "dark"));
  }

  function toggleDensity() {
    setDensity((current) => (current === "compact" ? "comfortable" : "compact"));
  }

  return (
    <UiContext.Provider value={{ theme, density, toggleTheme, toggleDensity }}>
      {children}
    </UiContext.Provider>
  );
}

// Hook shares this module with UiProvider.
// eslint-disable-next-line react-refresh/only-export-components
export function useUi() {
  const value = useContext(UiContext);
  if (!value) {
    throw new Error("useUi must be used within UiProvider");
  }
  return value;
}
