import { initializeApp } from "firebase/app";
import { getDatabase } from "firebase/database";

const firebaseConfig = {
  apiKey: import.meta.env.VITE_FIREBASE_API_KEY,
  databaseURL: import.meta.env.VITE_FIREBASE_DATABASE_URL,
};

if (!firebaseConfig.apiKey || !firebaseConfig.databaseURL) {
  throw new Error(
    "Missing Firebase env vars. Set VITE_FIREBASE_API_KEY and VITE_FIREBASE_DATABASE_URL in a .env file."
  );
}

const app = initializeApp(firebaseConfig);
export const db = getDatabase(app);
