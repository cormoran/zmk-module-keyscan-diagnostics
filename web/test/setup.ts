// jest-dom adds custom jest matchers for asserting on DOM nodes.
import "@testing-library/jest-dom";
import { TextEncoder, TextDecoder } from "util";

// Add TextEncoder/TextDecoder polyfills for jest environment
global.TextEncoder = TextEncoder as any;
global.TextDecoder = TextDecoder as any;
