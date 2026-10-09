import { z } from 'zod';
import { t } from '../i18n';

// Alerts settings (split from configSchema.ts to keep files short).

const httpUrl = z.string().regex(/^https?:\/\/.+/, t('validation.configSchema.mustStartWithHttpOr'));

export const alertsConfigSchema = z
  .object({
    enabled: z.boolean(),
    events: z.record(
      z.string(),
      z.object({
        level: z.number().int().min(0).max(4),
        sound: z
          .string()
          .max(32)
          .regex(/^[a-z0-9_-]*$/i, t('validation.configSchema.notAPushoverSoundName')),
        title: z.string().max(80, t('validation.configSchema.titlesAreUpTo80')).optional(),
        message: z.string().max(240, t('validation.configSchema.messagesAreUpTo240')).optional(),
      }),
    ),
    dewRiskMarginC: z.number().min(0).max(10),
    clearSkyCloudPercent: z.number().min(0).max(100),
    cloudedOverCloudPercent: z.number().min(0).max(100),
    skyNightOnly: z.boolean(),
    safetyNightOnly: z.boolean().optional(),
    sendMode: z.enum(['any', 'whileConnected']).optional(),
    armWithAlpaca: z.boolean().optional(),
    clientSilentSafetySeconds: z
      .number()
      .int()
      .min(30, t('validation.configSchema.atLeast30Seconds'))
      .max(3600, t('validation.configSchema.atMost60Minutes'))
      .optional(),
    clientSilentWeatherSeconds: z
      .number()
      .int()
      .min(30, t('validation.configSchema.atLeast30Seconds'))
      .max(3600, t('validation.configSchema.atMost60Minutes'))
      .optional(),
    nightSunAltitudeDeg: z.number().min(-20).max(0),
    cooldownSeconds: z.number().int().min(0).max(86400, t('validation.configSchema.mustBeAtMost24')),
    pushover: z.object({
      enabled: z.boolean(),
      userKey: z.string(),
      appToken: z.string(),
      sound: z.string(),
    }),
    ntfy: z.object({ enabled: z.boolean(), server: z.string(), topic: z.string(), token: z.string() }),
    webhook: z.object({ enabled: z.boolean(), url: z.string(), authHeader: z.string(), insecureTls: z.boolean() }),
    mqtt: z.object({ enabled: z.boolean() }),
  })
  .superRefine((data, ctx) => {
    if (data.cloudedOverCloudPercent <= data.clearSkyCloudPercent) {
      ctx.addIssue({ code: 'custom', path: ['cloudedOverCloudPercent'], message: t('validation.configSchema.mustBeAboveTheClear') });
    }
    // Pushover keys are exactly 30 letters/digits; "********" is the stored, masked value.
    const pushoverKey = /^([A-Za-z0-9]{30}|\*{8})$/;
    if (data.pushover.enabled && !pushoverKey.test(data.pushover.userKey.trim())) {
      ctx.addIssue({
        code: 'custom',
        path: ['pushover', 'userKey'],
        message: t('validation.configSchema.theUserKeyIsThe'),
      });
    }
    if (data.pushover.enabled && !pushoverKey.test(data.pushover.appToken.trim())) {
      ctx.addIssue({
        code: 'custom',
        path: ['pushover', 'appToken'],
        message: t('validation.configSchema.theAppTokenIsThe'),
      });
    }
    if (data.ntfy.enabled) {
      if (!httpUrl.safeParse(data.ntfy.server).success) {
        ctx.addIssue({ code: 'custom', path: ['ntfy', 'server'], message: t('validation.configSchema.serverMustStartWithHttp') });
      }
      if (!data.ntfy.topic) {
        ctx.addIssue({ code: 'custom', path: ['ntfy', 'topic'], message: t('validation.configSchema.topicIsRequired') });
      }
    }
    if (data.webhook.enabled && !httpUrl.safeParse(data.webhook.url).success) {
      ctx.addIssue({ code: 'custom', path: ['webhook', 'url'], message: t('validation.configSchema.webhookUrlMustStartWith') });
    }
  });
