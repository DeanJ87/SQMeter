// Types for the generated device core (tools/demo-core/build.sh).
interface SqmCoreModule {
  EmulatedDevice: new (identityJson: string) => unknown;
}
declare const createSqmCore: (options?: Record<string, unknown>) => Promise<SqmCoreModule>;
export default createSqmCore;
