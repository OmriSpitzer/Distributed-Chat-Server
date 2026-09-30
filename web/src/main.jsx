/**
 * Main entry point
 * @brief Main entry point for the web application.
 * @date 2026-09-29
 * 
 * Main entry point for the web application, that renders the root component into the DOM.
 */

import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import './index.css'
import App from './App.jsx'
import { UiProvider } from './contexts/UiContext.jsx'

createRoot(document.getElementById('root')).render(
  <StrictMode>
    <UiProvider>
      <App />
    </UiProvider>
  </StrictMode>,
)
