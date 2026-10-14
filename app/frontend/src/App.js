import "@/App.css";
import { Toaster } from "sonner";
import Dashboard from "@/pages/Dashboard";

function App() {
  return (
    <div className="App">
      <Dashboard />
      <Toaster theme="light" position="bottom-right" richColors closeButton />
    </div>
  );
}

export default App;
